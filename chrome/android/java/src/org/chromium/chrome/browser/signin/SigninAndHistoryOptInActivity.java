// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.firstrun.FirstRunActivityBase;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.HistoryOptInMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.UpgradePromoCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The activity that host post-UNO sign-in flows. This activity is semi-transparent, and views for
 * different sign-in flow steps will be hosted by it, according to the account's state and the flow
 * type.
 *
 * <p>For most cases, the flow is contains a sign-in bottom sheet, and a history sync opt-in dialog
 * shown after sign-in completion.
 *
 * <p>The activity may also hold the re-FRE which consists of a fullscreen sign-in dialog followed
 * by the history sync opt-in. This is why the dependency on {@link FirstRunActivityBase} is needed.
 */
public class SigninAndHistoryOptInActivity extends FirstRunActivityBase
        implements SigninAndHistoryOptInCoordinator.Delegate, UpgradePromoCoordinator.Delegate {
    private static final String ARGUMENT_ACCESS_POINT = "SigninAndHistoryOptInActivity.AccessPoint";
    private static final String ARGUMENT_BOTTOM_SHEET_STRINGS =
            "SigninAndHistoryOptInActivity.BottomSheetStrings";
    private static final String ARGUMENT_NO_ACCOUNT_SIGNIN_MODE =
            "SigninAndHistoryOptInActivity.NoAccountSigninMode";
    private static final String ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE =
            "SigninAndHistoryOptInActivity.WithAccountSigninMode";
    private static final String ARGUMENT_HISTORY_OPT_IN_MODE =
            "SigninAndHistoryOptInActivity.HistoryOptInMode";
    private static final String ARGUMENT_IS_HISTORY_SYNC_DEDICATED_FLOW =
            "SigninAndHistoryOptInActivity.IsHistorySyncDedicatedFlow";
    private static final String ARGUMENT_IS_UPGRADE_PROMO =
            "SigninAndHistoryOptInActivity.IsUpgradePromo";

    private final OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();
    // TODO(b/41493788): Move this to FirstRunActivityBase
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();
    // These two coordinators are mutually exclusive: if one is initialized the other should be
    // null.
    // TODO(b/41493788): Consider making each of these implement a common interface to skip the
    // redundancy.
    private SigninAndHistoryOptInCoordinator mCoordinator;
    private UpgradePromoCoordinator mUpgradePromoCoordinator;

    @Override
    protected void onPreCreate() {
        // Temporarily ensure that the native is initialized before calling super.onCreate().
        // TODO(https://crbug.com/1520783): Handle the case where the UI is shown before the end of
        // native initialization.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
    }

    @Override
    public void triggerLayoutInflation() {
        super.triggerLayoutInflation();

        Intent intent = getIntent();
        if (intent.getBooleanExtra(ARGUMENT_IS_UPGRADE_PROMO, false)) {
            mUpgradePromoCoordinator =
                    new UpgradePromoCoordinator(
                            this,
                            getModalDialogManager(),
                            getProfileProviderSupplier(),
                            PrivacyPreferencesManagerImpl.getInstance(),
                            this);

            setContentView(mUpgradePromoCoordinator.getViewSwitcher());
            onInitialLayoutInflationComplete();
            return;
        }

        int signinAccessPoint = intent.getIntExtra(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert signinAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";
        AccountPickerBottomSheetStrings bottomSheetStrings =
                (AccountPickerBottomSheetStrings)
                        intent.getParcelableExtra(ARGUMENT_BOTTOM_SHEET_STRINGS);
        @NoAccountSigninMode
        int noAccountSigninMode =
                intent.getIntExtra(
                        ARGUMENT_NO_ACCOUNT_SIGNIN_MODE, NoAccountSigninMode.ADD_ACCOUNT);
        @WithAccountSigninMode
        int withAccountSigninMode =
                intent.getIntExtra(
                        ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE,
                        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET);
        @HistoryOptInMode
        int historyOptInMode =
                intent.getIntExtra(ARGUMENT_HISTORY_OPT_IN_MODE, HistoryOptInMode.OPTIONAL);
        boolean isHistorySyncDedicatedFlow =
                intent.getBooleanExtra(ARGUMENT_IS_HISTORY_SYNC_DEDICATED_FLOW, false);

        mCoordinator =
                new SigninAndHistoryOptInCoordinator(
                        getWindowAndroid(),
                        this,
                        this,
                        DeviceLockActivityLauncherImpl.get(),
                        mProfileSupplier,
                        getModalDialogManagerSupplier(),
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        historyOptInMode,
                        signinAccessPoint,
                        isHistorySyncDedicatedFlow);

        setContentView(mCoordinator.getView());
        onInitialLayoutInflationComplete();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        ActivityProfileProvider profileProvider =
                new ActivityProfileProvider(getLifecycleDispatcher()) {
                    @Nullable
                    @Override
                    protected OTRProfileID createOffTheRecordProfileID() {
                        throw new IllegalStateException(
                                "Attempting to access incognito in the sign-in & history sync"
                                        + " opt-in flow");
                    }
                };

        profileProvider.onAvailable(
                (provider) -> {
                    mProfileSupplier.set(profileProvider.get().getOriginalProfile());
                });
        return profileProvider;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this, /* listenToActivityState= */ true, getIntentRequestTracker());
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitializationPromise.fulfill(null);
    }

    @Override
    public void onFlowComplete() {
        finish();
        // Override activity animation to avoid visual glitches due to the semi-transparent
        // background.
        overridePendingTransition(0, android.R.anim.fade_out);
    }

    @Override
    public void performOnConfigurationChanged(Configuration newConfig) {
        super.performOnConfigurationChanged(newConfig);
        if (mCoordinator != null) {
            mCoordinator.switchHistorySyncLayout();
        } else {
            mUpgradePromoCoordinator.recreateLayoutAfterConfigurationChange();
            setContentView(mUpgradePromoCoordinator.getViewSwitcher());
        }
    }

    @Override
    protected void onDestroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        if (mUpgradePromoCoordinator != null) {
            mUpgradePromoCoordinator.destroy();
        }
        super.onDestroy();
    }

    @Override
    public int handleBackPress() {
        // TODO(b/41493788): Implement this method to handle back press correctly from the re-FRE
        // and the usual flow.
        return BackPressResult.UNKNOWN;
    }

    @Override
    public int getSecondaryActivity() {
        // TODO(b/41493788): Move the logic from the coordinator here (or vice-versa) to avoid
        // redundant code.
        return SecondaryActivityBackPressUma.SecondaryActivity.SIGNIN_AND_HISTORY_OPT_IN;
    }

    public static @NonNull Intent createIntent(
            @NonNull Context context,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int signinAccessPoint) {
        assert bottomSheetStrings != null;

        Intent intent = new Intent(context, SigninAndHistoryOptInActivity.class);
        intent.putExtra(ARGUMENT_BOTTOM_SHEET_STRINGS, bottomSheetStrings);
        intent.putExtra(ARGUMENT_NO_ACCOUNT_SIGNIN_MODE, noAccountSigninMode);
        intent.putExtra(ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE, withAccountSigninMode);
        intent.putExtra(ARGUMENT_HISTORY_OPT_IN_MODE, historyOptInMode);
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        return intent;
    }

    public static @NonNull Intent createIntentForDedicatedFlow(
            Context context,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent =
                createIntent(
                        context,
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        HistoryOptInMode.REQUIRED,
                        signinAccessPoint);
        intent.putExtra(ARGUMENT_IS_HISTORY_SYNC_DEDICATED_FLOW, true);
        return intent;
    }

    public static @NonNull Intent createIntentForUpgradePromo(Context context) {
        Intent intent = new Intent(context, SigninAndHistoryOptInActivity.class);
        intent.putExtra(ARGUMENT_IS_UPGRADE_PROMO, true);
        return intent;
    }

    /** Implements {@link UpgradePromoCoordinator.Delegate} */
    @Override
    public void addAccountInUpgradePromo() {
        // TODO(b/41493788): Implement this method
    }

    /** Implements {@link UpgradePromoCoordinator.Delegate} */
    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return mNativeInitializationPromise;
    }
}
