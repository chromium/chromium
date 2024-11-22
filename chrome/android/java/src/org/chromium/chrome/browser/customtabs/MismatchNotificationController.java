// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** A controller for the account mismatched notice message. */
public class MismatchNotificationController
        implements SigninManager.SignInStateObserver, AccountsChangeObserver {
    // LINT.IfChange(SuppressedReason)
    /** Used to record Signin.CctAccountMismatchNoticeSuppressed histogram. */
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        SuppressedReason.ACCOUNT_LIST_NOT_YET_AVAILABLE,
        SuppressedReason.NOTICE_DISPLAY_LIMIT_MET,
        SuppressedReason.NOTICE_DISMISSED_MULTIPLE_TIMES,
        SuppressedReason.NOTICE_DISPLAYED_RECENTLY,
        SuppressedReason.FRE_COMPLETED_RECENTLY,
        SuppressedReason.CCT_IS_OFF_THE_RECORD,
        SuppressedReason.MAX
    })
    public @interface SuppressedReason {
        int ACCOUNT_LIST_NOT_YET_AVAILABLE = 0;
        int NOTICE_DISPLAY_LIMIT_MET = 1;
        int NOTICE_DISMISSED_MULTIPLE_TIMES = 2;
        int NOTICE_DISPLAYED_RECENTLY = 3;
        int FRE_COMPLETED_RECENTLY = 4;
        int CCT_IS_OFF_THE_RECORD = 5;
        int MAX = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SuppressedReason)
    private final WindowAndroid mWindowAndroid;
    private final Profile mProfile;
    private final String mAppAccountEmail;
    private final CoreAccountId mAppAccountId;
    private final AccountManagerFacade mAccountManagerFacade;
    private final SigninManager mSigninManager;

    private PropertyModel mMessageProperties;
    private boolean mMessageReachedFullyVisible;

    @Nullable
    private static MismatchNotificationController sMismatchNotificationControllerForTesting;

    public static MismatchNotificationController get(
            WindowAndroid windowAndroid, Profile profile, String appAccountName) {
        if (sMismatchNotificationControllerForTesting != null) {
            return sMismatchNotificationControllerForTesting;
        }
        return new MismatchNotificationController(windowAndroid, profile, appAccountName);
    }

    private MismatchNotificationController(
            WindowAndroid windowAndroid, Profile profile, String appAccountName) {
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mAppAccountEmail = appAccountName;
        mAppAccountId =
                IdentityServicesProvider.get()
                        .getIdentityManager(profile)
                        .findExtendedAccountInfoByEmailAddress(appAccountName)
                        .getId();
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mSigninManager.addSignInStateObserver(this);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
    }

    public static void recordMismatchNoticeSuppressedHistogram(
            @SuppressedReason int suppressedReason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.CctAccountMismatchNoticeSuppressed",
                suppressedReason,
                SuppressedReason.MAX);
    }

    public void showSignedOutMessage(Context context, Callback<Integer> onClose) {
        mMessageProperties =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.CCT_ACCOUNT_MISMATCH_NOTICE)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.custom_tabs_signed_out_message_button))
                        .with(
                                MessageBannerProperties.TITLE,
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.custom_tabs_signed_out_message_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                ContextUtils.getApplicationContext()
                                        .getString(
                                                R.string.custom_tabs_signed_out_message_subtitle))
                        // TODO(crbug.com/369562441): Update the image once specs are finalized.
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.fre_product_logo)
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> handlePrimaryAction(context))
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (reason) -> handleDismissed(reason, onClose))
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, this::onFullyVisible)
                        .build();

        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        dispatcher.enqueueWindowScopedMessage(mMessageProperties, /* highPriority= */ false);
    }

    private @PrimaryActionClickBehavior int handlePrimaryAction(Context context) {
        SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher =
                SigninAndHistorySyncActivityLauncherImpl.get();

        // TODO(crbug.com/369562441): Update these strings once specs are finalized.
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                org.chromium.chrome.browser.ui.signin.R.string
                                        .signin_account_picker_bottom_sheet_title)
                        .setSubtitleStringId(
                                org.chromium.chrome.browser.ui.signin.R.string
                                        .signin_account_picker_bottom_sheet_benefits_subtitle)
                        .build();

        @Nullable
        Intent intent =
                signinAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        context,
                        mProfile,
                        bottomSheetStrings,
                        BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode.NO_SIGNIN,
                        BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        HistorySyncConfig.OptInMode.NONE,
                        SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION,
                        mAppAccountId);
        if (intent != null) {
            context.startActivity(intent);
        }

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void onFullyVisible(boolean visible) {
        if (visible) mMessageReachedFullyVisible = true;
    }

    private void handleDismissed(Integer reason, Callback<Integer> onClose) {
        CloseType closeType;
        if (!mMessageReachedFullyVisible) {
            closeType = CloseType.UNKNOWN;
        } else {
            closeType =
                    switch (reason) {
                        case DismissReason.PRIMARY_ACTION,
                                DismissReason.SECONDARY_ACTION -> CloseType.ACCEPTED;
                        case DismissReason.GESTURE -> CloseType.DISMISSED;
                        case DismissReason.TIMER -> CloseType.TIMED_OUT;
                            // Rest of the cases has no user intervention, thus viewed as time-out.
                        default -> CloseType.TIMED_OUT;
                    };
        }
        onClose.onResult(closeType.getNumber());

        destroy();
    }

    private void destroy() {
        mAccountManagerFacade.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
    }

    @Override
    public void onSignedIn() {
        dismissMessage();
    }

    @Override
    public void onCoreAccountInfosChanged() {
        assert mAccountManagerFacade.getCoreAccountInfos().isFulfilled();
        final List<CoreAccountInfo> coreAccountInfos =
                mAccountManagerFacade.getCoreAccountInfos().getResult();
        if (AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, mAppAccountEmail) == null) {
            dismissMessage();
        }
    }

    private void dismissMessage() {
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        assert dispatcher != null;
        dispatcher.dismissMessage(mMessageProperties, DismissReason.DISMISSED_BY_FEATURE);
    }

    public static void setInstanceForTesting(
            MismatchNotificationController mismatchNotificationController) {
        sMismatchNotificationControllerForTesting = mismatchNotificationController;
    }

    WindowAndroid getWindowForTesting() {
        return mWindowAndroid;
    }

    boolean wasShownForTesting() {
        return mMessageReachedFullyVisible;
    }
}
