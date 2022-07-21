// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_CARD_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_SHOW_COUNT;

import android.content.Context;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/**
 * Message service class to show the Incognito re-auth promo inside the incognito
 * tab switcher.
 *
 * TODO(crbug.com/1227656): Add review logic and integrate this class.
 */
public class IncognitoReauthPromoMessageService extends MessageService {
    /**
     * TODO(crbug.com/1227656): Remove this when we support all the Android versions.
     */
    @VisibleForTesting
    public static Boolean sIsPromoEnabledForTesting;

    private final int mMaxPromoMessageCount = 10;
    /**
     *  TODO(crbug.com/1148020): Currently every time entering the tab switcher,
     *  {@link ResetHandler.resetWithTabs} will be called twice if
     *  {@link TabUiFeatureUtilities#isTabToGtsAnimationEnabled} returns true, see
     *  {@link TabSwitcherMediator#prepareOverview}.
     */
    private final int mPrepareMessageEnteringTabSwitcher;

    @VisibleForTesting
    public final int mMaximumPromoShowCountLimit;

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    class IncognitoReauthMessageData implements MessageData {
        private final MessageCardView.ReviewActionProvider mReviewActionProvider;
        private final MessageCardView.DismissActionProvider mDismissActionProvider;

        IncognitoReauthMessageData(
                @NonNull MessageCardView.ReviewActionProvider reviewActionProvider,
                @NonNull MessageCardView.DismissActionProvider dismissActionProvider) {
            mReviewActionProvider = reviewActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return mReviewActionProvider;
        }

        MessageCardView.DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    private final @NonNull Profile mProfile;
    private final @NonNull Context mContext;
    private final @NonNull SharedPreferencesManager mSharedPreferencesManager;
    private final @NonNull SnackbarManager mSnackBarManager;

    /**
     * @param mMessageType The type of the message.
     * @param profile {@link Profile} to use to check the re-auth status.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} to query about re-auth
     *         promo shared preference.
     * @param snackbarManager {@link SnackbarManager} to show a snack-bar after a successful review
     * @param isTabToGtsAnimationEnabledSupplier {@link Supplier<Boolean>} indicating whether tab to
     *         grid tab switcher animation is enabled. This affects the maximum promo shown count.
     */
    IncognitoReauthPromoMessageService(int mMessageType, @NonNull Profile profile,
            @NonNull Context context, @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<Boolean> isTabToGtsAnimationEnabledSupplier) {
        super(mMessageType);
        mProfile = profile;
        mContext = context;
        mSharedPreferencesManager = sharedPreferencesManager;
        mSnackBarManager = snackbarManager;
        mPrepareMessageEnteringTabSwitcher = isTabToGtsAnimationEnabledSupplier.get() ? 2 : 1;
        mMaximumPromoShowCountLimit = mMaxPromoMessageCount * mPrepareMessageEnteringTabSwitcher;
    }

    @VisibleForTesting
    void dismiss() {
        sendInvalidNotification();
        disableIncognitoReauthPromoMessage();
    }

    void increasePromoShowCountAndMayDisableIfCountExceeds() {
        if (getPromoShowCount() > mMaximumPromoShowCountLimit) {
            dismiss();
            return;
        }

        mSharedPreferencesManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT, getPromoShowCount() + 1);
    }

    int getPromoShowCount() {
        return mSharedPreferencesManager.readInt(INCOGNITO_REAUTH_PROMO_SHOW_COUNT, 0);
    }

    /**
     * Prepares a re-auth promo message notifying a new message is available.
     *
     * @return A boolean indicating if the promo message was successfully prepared or not.
     */
    @VisibleForTesting
    boolean preparePromoMessage() {
        if (!isIncognitoReauthPromoMessageEnabled(mProfile)) return false;

        // We also need to ensure an "equality" check because, we only increase the count of the
        // promo when we actually show it in the tab switcher. At the |mMaximumPromoShowCountLimit|
        // time (the last time) we show the promo, we haven't yet dismissed the dialog.
        // Now, if the user recreates the Chrome Activity instance, the count will be read as
        // |mMaximumPromoShowCountLimit| at this point, so we should dismiss the promo.
        if (getPromoShowCount() >= mMaximumPromoShowCountLimit) {
            dismiss();
            return false;
        }

        sendAvailabilityNotification(
                new IncognitoReauthMessageData(this::review, (int messageType) -> dismiss()));
        return true;
    }

    @Override
    public void addObserver(MessageObserver observer) {
        super.addObserver(observer);
        preparePromoMessage();
    }

    /**
     * Provides the functionality to the {@link MessageCardView.ReviewActionProvider}
     */
    public void review() {
        // TODO(crbug.com/1227656): Add implementation for the review action.
    }

    /**
     * A method to indicate whether the Incognito re-auth promo is enabled or not.
     *
     * @param profile {@link Profile} to use to check the re-auth status.
     * @return True, if the incognito re-auth promo message is enabled, false otherwise.
     */
    public boolean isIncognitoReauthPromoMessageEnabled(Profile profile) {
        // To support lower android versions where we support running the render tests.
        if (sIsPromoEnabledForTesting != null) return sIsPromoEnabledForTesting;

        // The Chrome level Incognito lock setting is already enabled, so no use to show a promo for
        // that.
        if (IncognitoReauthManager.isIncognitoReauthEnabled(profile)) return false;
        // The Incognito re-auth feature is not enabled, so don't show the promo.
        if (!IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) return false;
        // The promo relies on turning on the Incognito lock setting on user's behalf but after a
        // device level authentication, which must be setup beforehand.
        // TODO(crbug.com/1227656): Remove the check on the API once all Android version is
        // supported.
        if ((Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
                || !IncognitoReauthSettingUtils.isDeviceScreenLockEnabled()) {
            return false;
        }

        return mSharedPreferencesManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true);
    }

    @VisibleForTesting
    public static void setIsPromoEnabledForTesting(@Nullable Boolean enabled) {
        sIsPromoEnabledForTesting = enabled;
    }

    private void disableIncognitoReauthPromoMessage() {
        mSharedPreferencesManager.writeBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false);
    }
}
