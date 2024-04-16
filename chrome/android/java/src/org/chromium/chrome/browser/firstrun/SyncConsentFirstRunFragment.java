// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.SyncConsentDelegate;
import org.chromium.chrome.browser.ui.signin.SyncConsentFragmentBase;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Implementation of {@link SyncConsentFragmentBase} for the first run experience. */
public class SyncConsentFirstRunFragment extends SyncConsentFragmentBase
        implements FirstRunFragment {
    // Per-page parameters:
    // TODO(crbug.com/40165044): Remove IS_CHILD_ACCOUNT
    public static final String IS_CHILD_ACCOUNT = "IsChildAccount";

    private final SyncConsentDelegate mSyncConsentDelegate;

    // Do not remove. Empty fragment constructor is required for re-creating the fragment from a
    // saved state bundle. See crbug.com/1225102
    public SyncConsentFirstRunFragment() {
        mSyncConsentDelegate =
                new SyncConsentDelegate() {
                    @Nullable
                    @Override
                    public WindowAndroid getWindowAndroid() {
                        FirstRunPageDelegate delegate = getPageDelegate();
                        if (delegate == null) return null;
                        return delegate.getWindowAndroid();
                    }

                    @Nullable
                    @Override
                    public Profile getProfile() {
                        FirstRunPageDelegate delegate = getPageDelegate();
                        if (delegate == null) return null;
                        return delegate.getProfileProviderSupplier().get().getOriginalProfile();
                    }
                };
    }

    @NonNull
    @Override
    protected SyncConsentDelegate getDelegate() {
        return mSyncConsentDelegate;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        final @Nullable CoreAccountInfo defaultAccount =
                AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
        final @Nullable String accountEmail =
                defaultAccount == null ? null : defaultAccount.getEmail();
        boolean isChild = getPageDelegate().getProperties().getBoolean(IS_CHILD_ACCOUNT, false);
        // TODO(crbug.com/40285057): Avoid sending `accountEmail` to create arguments. This class
        // uses the primary account from IdentityManager.
        setArguments(createArguments(SigninAccessPoint.START_PAGE, accountEmail, isChild));
    }

    @Override
    protected void onSyncRefused() {
        SigninPreferencesManager.getInstance().temporarilySuppressNewTabPagePromos();
        getPageDelegate().recordFreProgressHistogram(MobileFreProgress.SYNC_CONSENT_DISMISSED);
        getPageDelegate().advanceToNextPage();
    }

    @Override
    protected void onSyncAccepted(
            String accountName, boolean settingsClicked, SigninManager.SignInCallback callback) {
        // TODO(crbug.com/40217047): Once ENABLE_SYNC_IMMEDIATELY_IN_FRE launches, move these
        // metrics
        // elsewhere, so onSyncAccepted() is replaced with signinAndEnableSync() (common code).
        getPageDelegate().recordFreProgressHistogram(MobileFreProgress.SYNC_CONSENT_ACCEPTED);
        if (settingsClicked) {
            getPageDelegate()
                    .recordFreProgressHistogram(MobileFreProgress.SYNC_CONSENT_SETTINGS_LINK_CLICK);
        }

        // Enable sync now. Only call FirstRunSignInProcessor.scheduleOpeningSettings() later in
        // closeAndMaybeOpenSyncSettings(), because settings shouldn't open if
        // signinAndEnableSync() fails.
        if (!getPageDelegate().getProperties().getBoolean(IS_CHILD_ACCOUNT, false)) {
            signinAndEnableSync(accountName, settingsClicked, callback);
            return;
        }

        // Special case for child accounts. In rare cases, e.g. if Terms & Conditions is clicked,
        // SigninChecker might have been triggered before the FRE ends and started sign-in (the
        // ConsentLevel depends on AllowSyncOffForChildAccounts). In doubt, wait.
        Profile profile = getPageDelegate().getProfileProviderSupplier().get().getOriginalProfile();
        IdentityServicesProvider.get()
                .getSigninManager(profile)
                .runAfterOperationInProgress(
                        () -> {
                            CoreAccountInfo syncingAccount =
                                    IdentityServicesProvider.get()
                                            .getIdentityManager(profile)
                                            .getPrimaryAccountInfo(ConsentLevel.SYNC);
                            if (syncingAccount == null) {
                                signinAndEnableSync(accountName, settingsClicked, callback);
                                return;
                            }

                            if (!accountName.equals(syncingAccount.getEmail())) {
                                throw new IllegalStateException(
                                        "Child accounts should only be allowed to sync with a"
                                                + " single account");
                            }

                            // SigninChecker enabled sync already. Just open settings if needed.
                            closeAndMaybeOpenSyncSettings(settingsClicked);
                            callback.onSignInComplete();
                        });
    }

    @Override
    protected void closeAndMaybeOpenSyncSettings(boolean settingsClicked) {
        // Now that signinAndEnableSync() succeeded, signal whether FirstRunSignInProcessor should
        // open settings.
        if (settingsClicked) {
            FirstRunSignInProcessor.scheduleOpeningSettings();
        }
        getPageDelegate().advanceToNextPage();
    }

    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        View title = getView().findViewById(R.id.signin_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    protected void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
        final boolean selectedAccountDoesNotExist =
                (mSelectedAccountEmail != null
                        && AccountUtils.findCoreAccountInfoByEmail(
                                        coreAccountInfos, mSelectedAccountEmail)
                                == null);
        if (selectedAccountDoesNotExist) {
            // With MICe, there's no account picker and the sync consent is fixed for the signed
            // in account on welcome screen. If the signed-in account is removed, this page
            // no longer makes sense, so we abort the FRE here to allow users to restart FRE.
            getPageDelegate().abortFirstRunExperience();
            return;
        }
        super.updateAccounts(coreAccountInfos);
    }
}
