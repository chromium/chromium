// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.accounts.Account;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.ui.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * A controller for configuring the sign in promo. It sets up the sign in promo depending on the
 * context: whether there are any Google accounts on the device which have been previously signed in
 * or not. The controller also takes care of counting impressions, recording signin related user
 * actions and histograms.
 */
public class SigninPromoController {
    /**
     * Receives notifications when user clicks close button in the promo.
     */
    public interface OnDismissListener {
        /**
         * Action to be performed when the promo is being dismissed.
         */
        void onDismiss();
    }

    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int MAX_IMPRESSIONS_SETTINGS = 20;

    private @Nullable DisplayableProfileData mProfileData;
    private @Nullable ImpressionTracker mImpressionTracker;
    private final @AccessPoint int mAccessPoint;
    private final @Nullable String mImpressionCountName;
    private final String mImpressionUserActionName;
    private final String mImpressionWithAccountUserActionName;
    private final String mImpressionWithNoAccountUserActionName;
    private final String mSigninWithDefaultUserActionName;
    private final String mSigninNotDefaultUserActionName;
    private final String mSigninNewAccountUserActionName;
    private final @Nullable String mSyncPromoDismissedPreferenceTracker;
    private final @Nullable String mImpressionsTilDismissHistogramName;
    private final @Nullable String mImpressionsTilSigninButtonsHistogramName;
    private final @Nullable String mImpressionsTilXButtonHistogramName;
    private final @StringRes int mDescriptionStringId;
    private final @StringRes int mDescriptionStringIdNoAccount;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private boolean mWasDisplayed;
    private boolean mWasUsed;

    /**
     * Determines whether the Sync promo can be shown.
     * @param accessPoint The access point for which the impression limit is being checked.
     */
    public static boolean canShowSyncPromo(@AccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return canShowBookmarkPromo();
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                return canShowNTPPromo();
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit or dismiss button in Recent Tabs promo.
                return true;
            case SigninAccessPoint.SETTINGS:
                return canShowSettingsPromo();
            default:
                assert false : "Unexpected value for access point: " + accessPoint;
                return false;
        }
    }

    private static boolean canShowBookmarkPromo() {
        boolean isPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);
        return getSigninPromoImpressionsCountBookmarks() < MAX_IMPRESSIONS_BOOKMARKS
                && !isPromoDismissed;
    }

    /**
     * Expires the promo card if Chrome has been in the background long enough. We're using the
     * "Start Surface" concept of session here which is if the app has been in the background for X
     * amount of time. Called from #onStartWithNative, after which the time stored in {@link
     * ChromeInactivityTracker} is expired.
     *
     * @param timeSinceLastBackgroundedMs The time since Chrome has sent into the background.
     */
    public static void maybeExpireNTPPromo(long timeSinceLastBackgroundedMs) {
        long timeSinceLastBackgroundedLimitMs =
                StartSurfaceConfiguration.SIGN_IN_PROMO_SHOW_SINCE_LAST_BACKGROUNDED_LIMIT_MS
                        .getValue();
        // If |timeSinceLastBackgroundedLimitMs| is not -1, which means we have a limit on last
        // background time, check and record whether the current time is expired.
        if (timeSinceLastBackgroundedLimitMs > 0
                && timeSinceLastBackgroundedMs > timeSinceLastBackgroundedLimitMs) {
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_EXPIRED, true);
        }
    }

    private static boolean canShowNTPPromo() {
        int maxImpressions = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD, "MaxSigninPromoImpressions",
                Integer.MAX_VALUE);
        if (SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP)
                >= maxImpressions) {
            return false;
        }

        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_EXPIRED, false)) {
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS)) {
            return false;
        }
        final @Nullable Account visibleAccount = getVisibleAccount();
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        return visibleAccount == null
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.MINOR_MODE_SUPPORT)
                || accountManagerFacade.canOfferExtendedSyncPromos(visibleAccount).or(false);
    }

    private static boolean canShowSettingsPromo() {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        boolean isPromoDismissed = preferencesManager.readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
        return preferencesManager.readInt(
                       ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS)
                < MAX_IMPRESSIONS_SETTINGS
                && !isPromoDismissed;
    }

    // Find the visible account for sync promos
    private static @Nullable Account getVisibleAccount() {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        @Nullable
        Account visibleAccount = CoreAccountInfo.getAndroidAccountFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultAccountIfFulfilled(accountManagerFacade.getAccounts());
        }
        return visibleAccount;
    }

    /**
     * Creates a new SigninPromoController.
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     * @param syncConsentActivityLauncher Launcher of {@link SyncConsentActivity}.
     */
    public SigninPromoController(
            @AccessPoint int accessPoint, SyncConsentActivityLauncher syncConsentActivityLauncher) {
        mAccessPoint = accessPoint;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionCountName =
                        ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS;
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromBookmarkManager";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromBookmarkManager";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromBookmarkManager";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromBookmarkManager";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromBookmarkManager";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED;
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton";
                mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_bookmarks_no_account;
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP;
                mImpressionUserActionName = "Signin_Impression_FromNTPContentSuggestions";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromNTPContentSuggestions";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromNTPContentSuggestions";
                mSigninWithDefaultUserActionName =
                        "Signin_SigninWithDefault_FromNTPContentSuggestions";
                mSigninNotDefaultUserActionName =
                        "Signin_SigninNotDefault_FromNTPContentSuggestions";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromNTPContentSuggestions";
                mSyncPromoDismissedPreferenceTracker = null;
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
                mDescriptionStringId = R.string.signin_promo_description_ntp_content_suggestions;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_ntp_content_suggestions_no_account;
                break;
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                mImpressionCountName = null;
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromRecentTabs";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromRecentTabs";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromRecentTabs";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromRecentTabs";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromRecentTabs";
                mSyncPromoDismissedPreferenceTracker = null;
                mImpressionsTilDismissHistogramName = null;
                mImpressionsTilSigninButtonsHistogramName = null;
                mImpressionsTilXButtonHistogramName = null;
                mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_recent_tabs_no_account;
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS;
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mImpressionWithAccountUserActionName = "Signin_ImpressionWithAccount_FromSettings";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromSettings";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromSettings";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromSettings";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromSettings";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED;
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss";
                mImpressionsTilSigninButtonsHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilSigninButtons";
                mImpressionsTilXButtonHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilXButton";
                mDescriptionStringId = R.string.signin_promo_description_settings;
                mDescriptionStringIdNoAccount =
                        R.string.signin_promo_description_settings_no_account;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /**
     * Sets up the sync promo view.
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SigninPromoController.OnDismissListener} to be set to the view.
     */
    public void setUpSyncPromoView(ProfileDataCache profileDataCache,
            PersonalizedSigninPromoView view, SigninPromoController.OnDismissListener listener) {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        assert !identityManager.hasPrimaryAccount(ConsentLevel.SYNC) : "Sync is already enabled!";

        final @Nullable Account visibleAccount = getVisibleAccount();
        // Set up the sync promo
        if (visibleAccount == null) {
            setupPromoView(view, /* profileData= */ null, listener);
            return;
        }
        setupPromoView(
                view, profileDataCache.getProfileDataOrDefault(visibleAccount.name), listener);
    }

    /**
     * Called when the signin promo is destroyed.
     */
    public void onPromoDestroyed() {
        if (!mWasDisplayed || mWasUsed || mImpressionsTilDismissHistogramName == null) {
            return;
        }
        RecordHistogram.recordCount100Histogram(
                mImpressionsTilDismissHistogramName, getNumImpressions());
    }

    /**
     * Configures the signin promo view and resets the impression tracker. If this controller has
     * been previously set up.
     * @param view The view in which the promo will be added.
     * @param profileData If not null, the promo will be configured to be in the hot state, using
     *         the account image, email and full name of the user to set the picture and the text of
     *         the promo appropriately. Otherwise, the promo will be in the cold state.
     * @param onDismissListener Listener which handles the action of dismissing the promo. A null
     *         onDismissListener marks that the promo is not dismissible and as a result the close
     *         button is hidden.
     */
    private void setupPromoView(PersonalizedSigninPromoView view,
            final @Nullable DisplayableProfileData profileData,
            final @Nullable OnDismissListener onDismissListener) {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(
                new OneShotImpressionListener(this::recordSigninPromoImpression));

        mProfileData = profileData;
        mWasDisplayed = true;
        if (mProfileData == null) {
            setupColdState(view);
        } else {
            setupHotState(view);
        }

        if (onDismissListener != null) {
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton().setOnClickListener(promoView -> {
                assert mImpressionsTilXButtonHistogramName != null;
                assert mSyncPromoDismissedPreferenceTracker != null;
                mWasUsed = true;
                RecordHistogram.recordCount100Histogram(
                        mImpressionsTilXButtonHistogramName, getNumImpressions());
                SharedPreferencesManager.getInstance().writeBoolean(
                        mSyncPromoDismissedPreferenceTracker, true);
                onDismissListener.onDismiss();
            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /**
     * Should be called when the view is not in use anymore (e.g. it's being recycled).
     */
    public void detach() {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    private void setupColdState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        view.getDescription().setText(mDescriptionStringIdNoAccount);

        view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        view.getPrimaryButton().setOnClickListener(v -> signinWithNewAccount(context));

        view.getSecondaryButton().setVisibility(View.GONE);
    }

    private void setupHotState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.signin_promo_account_image_size);

        view.getDescription().setText(mDescriptionStringId);

        view.getPrimaryButton().setOnClickListener(v -> signinWithDefaultAccount(context));
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
            view.getSecondaryButton().setVisibility(View.GONE);
        } else {
            final String primaryButtonText =
                    ChromeFeatureList.isEnabled(
                            ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY_PROMOS)
                    ? context.getString(R.string.signin_promo_continue_as,
                            mProfileData.getGivenNameOrFullNameOrEmail())
                    : context.getString(R.string.sync_promo_turn_on_sync);
            view.getPrimaryButton().setText(primaryButtonText);

            view.getSecondaryButton().setText(R.string.signin_promo_choose_another_account);
            view.getSecondaryButton().setOnClickListener(v -> signinWithNotDefaultAccount(context));
            view.getSecondaryButton().setVisibility(View.VISIBLE);
        }
    }

    private int getNumImpressions() {
        return SharedPreferencesManager.getInstance().readInt(mImpressionCountName);
    }

    private void signinWithNewAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNewAccountUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoAddAccountFlow(context, mAccessPoint);
    }

    private void signinWithDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninWithDefaultUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoDefaultFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void signinWithNotDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNotDefaultUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoChooseAccountFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void recordSigninButtonUsed() {
        mWasUsed = true;
        if (mImpressionsTilSigninButtonsHistogramName != null) {
            RecordHistogram.recordCount100Histogram(
                    mImpressionsTilSigninButtonsHistogramName, getNumImpressions());
        }
    }

    private void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private void recordSigninPromoImpression() {
        RecordUserAction.record(mImpressionUserActionName);
        if (mProfileData == null) {
            RecordUserAction.record(mImpressionWithNoAccountUserActionName);
        } else {
            RecordUserAction.record(mImpressionWithAccountUserActionName);
        }

        // If mImpressionCountName is not null then we should record impressions.
        if (mImpressionCountName != null) {
            SharedPreferencesManager.getInstance().incrementInt(mImpressionCountName);
        }
    }

    @VisibleForTesting
    public static void setSigninPromoImpressionsCountBookmarksForTests(int count) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS, count);
    }

    @VisibleForTesting
    public static void setPrefSigninPromoDeclinedBookmarksForTests(boolean isDeclined) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, isDeclined);
    }

    @VisibleForTesting
    public static int getSigninPromoImpressionsCountBookmarks() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS);
    }

    @VisibleForTesting
    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }
}
