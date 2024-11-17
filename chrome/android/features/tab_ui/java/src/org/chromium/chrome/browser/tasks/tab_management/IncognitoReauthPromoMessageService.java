// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_CARD_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_SHOW_COUNT;

import android.content.Context;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Message service class to show the Incognito re-auth promo inside the incognito tab switcher. */
public class IncognitoReauthPromoMessageService extends MessageService
        implements PauseResumeWithNativeObserver {
    /** TODO(crbug.com/40056462): Remove this when we support all the Android versions. */
    public static Boolean sIsPromoEnabledForTesting;

    /**
     * For instrumentation tests, we don't have the supported infrastructure to perform native
     * re-authentication. Therefore, setting this variable would skip the re-auth triggering and
     * simply call the next set of actions which would have been call, if the re-auth was indeed
     * successful.
     */
    private static Boolean sTriggerReviewActionWithoutReauthForTesting;

    @VisibleForTesting public final int mMaxPromoMessageCount = 10;

    /** The re-auth manager that is used to trigger the re-authentication. */
    private final @NonNull IncognitoReauthManager mIncognitoReauthManager;

    /** This is the data type that this MessageService is serving to its Observer. */
    static class IncognitoReauthMessageData implements MessageData {
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
    private final @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * A boolean to indicate when we had temporarily invalidated the promo card due to change of
     * Android level settings, which is needed to show the promo card. This is set to true in
     * such cases, where we need to re-prepare the message, in order for it to be shown again when
     * the Android level settings are on again. See #onResumeWithNative method.
     */
    private boolean mShouldTriggerPrepareMessage;

    /**
     * Represents the action type on the re-auth promo card.
     * DO NOT reorder items in this interface, because it's mirrored to UMA
     * (as IncognitoReauthPromoActionType).
     */
    @IntDef({
        IncognitoReauthPromoActionType.PROMO_ACCEPTED,
        IncognitoReauthPromoActionType.NO_THANKS,
        IncognitoReauthPromoActionType.PROMO_EXPIRED,
        IncognitoReauthPromoActionType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface IncognitoReauthPromoActionType {
        int PROMO_ACCEPTED = 0;
        int NO_THANKS = 1;
        int PROMO_EXPIRED = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * @param mMessageType The type of the message.
     * @param profile {@link Profile} to use to check the re-auth status.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} to query about re-auth
     *     promo shared preference.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} to trigger re-auth for the
     *     review action. This class takes ownership of the {@link IncognitoReauthManager} object
     *     and is responsible for its cleanup, see `destroy` method.
     * @param snackbarManager {@link SnackbarManager} to show a snack-bar after a successful review
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} dispacther to
     *     register listening to onResume events.
     */
    IncognitoReauthPromoMessageService(
            int mMessageType,
            @NonNull Profile profile,
            @NonNull Context context,
            @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull IncognitoReauthManager incognitoReauthManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        super(mMessageType);
        mProfile = profile;
        mContext = context;
        mSharedPreferencesManager = sharedPreferencesManager;
        mIncognitoReauthManager = incognitoReauthManager;
        mSnackBarManager = snackbarManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(this);
    }

    void destroy() {
        mIncognitoReauthManager.destroy();
        // Duplicate unregister is safe if dismiss() was invoked.
        mActivityLifecycleDispatcher.unregister(this);
    }

    @VisibleForTesting
    void dismiss() {
        sendInvalidNotification();
        disableIncognitoReauthPromoMessage();
        recordPromoImpressionsCount();

        // Once dismissed, we will never show the re-auth promo card again, so there's no need
        // to keep tracking the lifecycle events.
        mActivityLifecycleDispatcher.unregister(this);
    }

    void increasePromoShowCountAndMayDisableIfCountExceeds() {
        if (getPromoShowCount() > mMaxPromoMessageCount) {
            dismiss();

            RecordHistogram.recordEnumeratedHistogram(
                    "Android.IncognitoReauth.PromoAcceptedOrDismissed",
                    IncognitoReauthPromoActionType.PROMO_EXPIRED,
                    IncognitoReauthPromoActionType.NUM_ENTRIES);

            return;
        }

        increasePomoImpressionCount();
    }

    private void increasePomoImpressionCount() {
        mSharedPreferencesManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mSharedPreferencesManager.readInt(INCOGNITO_REAUTH_PROMO_SHOW_COUNT, 0) + 1);
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

        if (getPromoShowCount() >= mMaxPromoMessageCount) {
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

    void prepareSnackBarAndShow() {
        Snackbar snackbar =
                Snackbar.make(
                        mContext.getString(R.string.incognito_reauth_snackbar_text),
                        /* controller= */ null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_INCOGNITO_REAUTH_ENABLED_FROM_PROMO);
        // TODO(crbug.com/40056462):  Confirm with UX to see how the background color of the
        // snackbar needs to be revised.
        snackbar.setBackgroundColor(
                mContext.getColor(R.color.snackbar_background_color_baseline_dark));
        snackbar.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary_Baseline_Light);
        snackbar.setSingleLine(false);
        mSnackBarManager.showSnackbar(snackbar);
    }

    /**
     * A method to dismiss the re-auth promo, if the #isIncognitoReauthPromoMessageEnabled returns
     * false. This ensures any state change that may occur which results in the promo not being
     * enabled are accounted for when the users resumes back to ChromeTabbedActivity.
     */
    @Override
    public void onResumeWithNative() {
        updatePromoCardDismissalStatusIfNeeded();
    }

    @Override
    public void onPauseWithNative() {}

    /** Provides the functionality to the {@link MessageCardView.ReviewActionProvider} */
    public void review() {
        // Add a safety net in-case for potential multi window flows.
        if (!isIncognitoReauthPromoMessageEnabled(mProfile)) {
            updatePromoCardDismissalStatusIfNeeded();
            return;
        }

        // Do the core review action without triggering a re-authentication for testing only.
        if (sTriggerReviewActionWithoutReauthForTesting != null
                && sTriggerReviewActionWithoutReauthForTesting) {
            onAfterReviewActionSuccessful();
            return;
        }

        mIncognitoReauthManager.startReauthenticationFlow(
                new IncognitoReauthManager.IncognitoReauthCallback() {
                    @Override
                    public void onIncognitoReauthNotPossible() {}

                    @Override
                    public void onIncognitoReauthSuccess() {
                        onAfterReviewActionSuccessful();
                    }

                    @Override
                    public void onIncognitoReauthFailure() {}
                });
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
        // TODO(crbug.com/40056462): Remove the check on the API once all Android version is
        // supported.
        if ((Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
                || !IncognitoReauthSettingUtils.isDeviceScreenLockEnabled()) {
            return false;
        }

        return mSharedPreferencesManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true);
    }

    public static void setTriggerReviewActionWithoutReauthForTesting(boolean enabled) {
        sTriggerReviewActionWithoutReauthForTesting = enabled;
        ResettersForTesting.register(() -> sTriggerReviewActionWithoutReauthForTesting = null);
    }

    public static void setIsPromoEnabledForTesting(@Nullable Boolean enabled) {
        sIsPromoEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sIsPromoEnabledForTesting = null);
    }

    private void disableIncognitoReauthPromoMessage() {
        mSharedPreferencesManager.writeBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false);
    }

    /**
     * A method that dismisses the promo card and *conditionally* disables it if the conditions
     * which were met before to show a promo card is not true any more.
     *
     * <p>For the case when it only dismisses the card but doesn't disable it, it would prepare the
     * message again once it detects the promo card can now be enabled.
     *
     * <p>TODO(crbug.com/40056462): This method can dismiss the promo card abruptly w/o stating any
     * user-visible reasoning. This needs to be revisited with UX to see how best can we provide
     * user education in such scenarios.
     */
    private void updatePromoCardDismissalStatusIfNeeded() {
        if (!isIncognitoReauthPromoMessageEnabled(mProfile)) {
            // Here, if the user has enabled the Chrome level setting directly then we should
            // dismiss the promo completely.
            if (IncognitoReauthManager.isIncognitoReauthEnabled(mProfile)) {
                // This call also unregisters this lifecycle observer.
                dismiss();
            } else {
                // For all other cases, we only send an invalidate message but don't disable the
                // promo card completely.
                sendInvalidNotification();
                mShouldTriggerPrepareMessage = true;
            }
        } else {
            // The conditions are suitable to show a promo card again but only if we had
            // invalidated the message in the past.
            if (mShouldTriggerPrepareMessage) {
                preparePromoMessage();
            }
        }
    }

    /**
     * A method which gets fired when the re-authentication was successful after the review action.
     */
    private void onAfterReviewActionSuccessful() {
        UserPrefs.get(mProfile).setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.IncognitoReauth.PromoAcceptedOrDismissed",
                IncognitoReauthPromoActionType.PROMO_ACCEPTED,
                IncognitoReauthPromoActionType.NUM_ENTRIES);

        dismiss();
        prepareSnackBarAndShow();
    }

    private void recordPromoImpressionsCount() {
        RecordHistogram.recordExactLinearHistogram(
                "Android.IncognitoReauth.PromoImpressionAfterActionCount",
                getPromoShowCount(),
                mMaxPromoMessageCount);
    }
}
