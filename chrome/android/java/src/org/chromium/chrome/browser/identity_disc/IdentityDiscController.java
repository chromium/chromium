// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions (user sign-in state,
 * whether NTP is shown)
 */
@NullMarked
public class IdentityDiscController
        implements ProfileDataCache.Observer,
                IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                ButtonDataProvider,
                BottomSheetSigninAndHistorySyncCoordinator.Delegate {
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    // Activity is used by sign-in launcher to anchor the bottomsheet.
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final SnackbarManager mSnackbarManager;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;
    private @Nullable Profile mProfile;

    // We observe IdentityManager to receive primary account state change notifications.
    private @Nullable IdentityManager mIdentityManager;

    // SyncService is observed to update mIdentityError.
    private @Nullable SyncService mSyncService;

    // ProfileDataCache facilitates retrieving profile picture.
    private @Nullable ProfileDataCache mProfileDataCache;

    private final ButtonDataImpl mButtonData;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();

    private boolean mIsTabNtp;

    private @UserActionableError int mIdentityError = UserActionableError.NONE;

    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;

    /**
     * @param activity The hosting {@link Activity}, for sign-in bottom sheet and retrieving
     *     resources.
     * @param windowAndroid The {@link WindowAndroid} for the current window.
     * @param activityResultTracker The {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param deviceLockActivityLauncher The launcher for the device lock challenge.
     * @param profileSupplier The supplier of the current profile.
     * @param bottomSheetController The {@link BottomSheetController} to show the sign-in bottom
     *     sheet.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}.
     * @param snackbarManager The {@link SnackbarManager} to show sign-in/sign-out snackbars.
     */
    public IdentityDiscController(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager) {
        mContext = activity;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mProfileSupplier = profileSupplier;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManager = snackbarManager;

        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);

        mButtonData =
                new ButtonDataImpl(
                        /* canShow= */ false,
                        /* drawable= */ null,
                        /* onClickListener= */ view -> onClick(),
                        mContext.getString(R.string.accessibility_toolbar_btn_identity_disc),
                        /* supportsTinting= */ false,
                        new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.IDENTITY_DISC_FEATURE,
                                R.string.iph_identity_disc_text,
                                R.string.iph_identity_disc_accessibility_text),
                        /* isEnabled= */ true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        /* tooltipTextResId= */ Resources.ID_NULL);
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        mIsTabNtp = tab != null && tab.getNativePage() instanceof NewTabPage;
        if (!mIsTabNtp) {
            mButtonData.setCanShow(false);
            return mButtonData;
        }

        calculateButtonData();
        return mButtonData;
    }

    private void calculateButtonData() {
        if (mProfile == null) {
            assert !mButtonData.canShow();
            return;
        }

        String email = CoreAccountInfo.getEmailFrom(getSignedInAccountInfo());
        ensureProfileDataCache(mProfile);

        mButtonData.setButtonSpec(
                buttonSpecWithDrawableAndDescription(mButtonData.getButtonSpec(), email));
        mButtonData.setCanShow(true);
    }

    private ButtonSpec buttonSpecWithDrawableAndDescription(
            ButtonSpec buttonSpec, @Nullable String email) {
        Drawable drawable = getProfileImage(email);
        if (buttonSpec.getDrawable() == drawable) {
            return buttonSpec;
        }

        // `supportsTinting` must be false when showing the user's profile image or its placeholder,
        // to not alter the images colors in those cases.
        boolean shouldSupportTinting = email == null;
        String contentDescription = getContentDescription(email);
        return new ButtonSpec(
                drawable,
                buttonSpec.getOnClickListener(),
                /* onLongClickListener= */ null,
                contentDescription,
                shouldSupportTinting,
                buttonSpec.getIphCommandBuilder(),
                AdaptiveToolbarButtonVariant.UNKNOWN,
                buttonSpec.getActionChipLabelResId(),
                buttonSpec.getHoverTooltipTextId(),
                /* hasErrorBadge= */ mIdentityError != UserActionableError.NONE);
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    @EnsuresNonNull("mProfileDataCache")
    private void ensureProfileDataCache(Profile profile) {
        if (mProfileDataCache != null) return;

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assert identityManager != null;
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        mContext, identityManager, R.dimen.toolbar_identity_disc_size);
        mProfileDataCache.addObserver(this);
    }

    /**
     * Returns Profile picture Drawable. The size of the image corresponds to current visual state.
     */
    private Drawable getProfileImage(@Nullable String email) {
        assumeNonNull(mProfileDataCache);
        return email == null
                ? AppCompatResources.getDrawable(mContext, R.drawable.account_circle)
                : mProfileDataCache.getProfileDataOrDefault(email).getImage();
    }

    /** Resets ProfileDataCache. Used for flushing cached image when sign-in state changes. */
    private void resetIdentityDiscCache() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    /** Called after profile image becomes available. Updates the image on toolbar button. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        assert mProfileDataCache != null;

        if (profileData
                .getAccountEmail()
                .equals(CoreAccountInfo.getEmailFrom(getSignedInAccountInfo()))) {
            /*
             * We need to call {@link notifyObservers(false)} before calling
             * {@link notifyObservers(true)}. This is because {@link notifyObservers(true)} has been
             * called in {@link setProfile()}, and without calling {@link notifyObservers(false)},
             * the supplier implementation doesn't propagate the call. See https://cubug.com/1137535.
             */
            notifyObservers(false);
            notifyObservers(true);
        }
    }

    /**
     * Implements {@link IdentityManager.Observer}.
     *
     * <p>IdentityDisc should be always shown regardless of whether the user is signed out, signed
     * in or syncing.
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)) {
            case PrimaryAccountChangeEvent.Type.SET:
                notifyObservers(true);
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                resetIdentityDiscCache();
                notifyObservers(false);
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                break;
        }
    }

    /** Call to tear down dependencies. */
    @Override
    public void destroy() {

        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }

        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
            mIdentityManager = null;
        }

        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
            mSyncService = null;
        }

        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }

        mProfileSupplier.removeObserver(mProfileSupplierObserver);
        mProfile = null;
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        maybeUpdateIdentityErrorAndBadge();
    }

    @VisibleForTesting
    public @UserActionableError int getIdentityError() {
        return mIdentityError;
    }

    private void maybeUpdateIdentityErrorAndBadge() {
        if (mProfile == null) {
            return;
        }

        @UserActionableError int error = SyncSettingsUtils.getSyncError(mProfile);
        if (error == mIdentityError
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            // Nothing changed.
            return;
        }
        mIdentityError = error;

        CoreAccountInfo coreAccountInfo = getSignedInAccountInfo();
        if (coreAccountInfo != null) {
            ensureProfileDataCache(mProfile);
            mProfileDataCache.setBadge(
                    coreAccountInfo.getId(),
                    mIdentityError == UserActionableError.NONE
                            ? null
                            : ProfileDataCache.createToolbarIdentityDiscBadgeConfig(
                                    mContext, R.drawable.ic_error_badge_16dp));
        }
    }

    /**
     * Records IdentityDisc usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help. We also record the clicking actions on the profile icon in
     * histograms.
     */
    private void recordIdentityDiscUsed() {
        BrowserUiUtils.recordIdentityDiscClicked(mIsTabNtp);

        assert mProfile != null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }

    /**
     * Returns the account info of mIdentityManager if current profile is regular, and null for
     * off-the-record ones.
     *
     * @return account info for the current profile. Returns null for OTR profile.
     */
    private @Nullable CoreAccountInfo getSignedInAccountInfo() {
        return mIdentityManager != null
                ? mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                : null;
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, as set to null if profile is off-the-record.
     */
    private void setProfile(Profile profile) {
        mProfile = profile;

        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }

        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
        }

        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }

        if (profile.isOffTheRecord()) {
            mIdentityManager = null;
            mSyncService = null;
        } else {
            mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
            assumeNonNull(mIdentityManager);
            mIdentityManager.addObserver(this);
            calculateButtonData();
            initializeSigninCoordinator();

            mSyncService = SyncServiceFactory.getForProfile(profile);
            if (mSyncService != null) {
                mSyncService.addSyncStateChangedListener(this);
                maybeUpdateIdentityErrorAndBadge();
            }

            notifyObservers(true);
        }
    }

    private String getContentDescription(@Nullable String email) {
        if (email == null) {
            return mContext.getString(R.string.accessibility_toolbar_btn_signed_out_identity_disc);
        }

        assumeNonNull(mProfileDataCache);
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(email);
        String userName = profileData.getFullName();
        if (profileData.hasDisplayableEmailAddress()) {
            return mContext.getString(
                    mIdentityError == UserActionableError.NONE
                            ? R.string.accessibility_toolbar_btn_identity_disc_with_name_and_email
                            : R.string
                                    .accessibility_toolbar_btn_identity_disc_error_with_name_and_email,
                    userName,
                    email);
        }

        return mContext.getString(
                mIdentityError == UserActionableError.NONE
                        ? R.string.accessibility_toolbar_btn_identity_disc_with_name
                        : R.string.accessibility_toolbar_btn_identity_disc_error_with_name,
                userName);
    }

    @VisibleForTesting
    void onClick() {
        if (mProfile == null) {
            return;
        }
        recordIdentityDiscUsed();

        Profile originalProfile = mProfile.getOriginalProfile();
        if (getSignedInAccountInfo() == null
                && UserPrefs.get(originalProfile).getBoolean(Pref.SIGNIN_ALLOWED)) {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    mContext.getString(
                                            R.string.signin_account_picker_bottom_sheet_title))
                            .setSubtitleString(
                                    mContext.getString(
                                            R.string
                                                    .signin_account_picker_bottom_sheet_benefits_subtitle))
                            .build();
            BottomSheetSigninAndHistorySyncConfig config =
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                                    bottomSheetStrings,
                                    NoAccountSigninMode.BOTTOM_SHEET,
                                    WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    HistorySyncConfig.OptInMode.OPTIONAL,
                                    mContext.getString(R.string.history_sync_title),
                                    mContext.getString(R.string.history_sync_subtitle))
                            .signinSurveyType(
                                    SigninSurveyController.SigninSurveyType.NTP_SIGNIN_BUTTON)
                            .build();
            if (mSigninCoordinator != null) {
                mSigninCoordinator.startSigninFlow(config);
            } else {
                @Nullable Intent intent =
                        SigninAndHistorySyncActivityLauncherImpl.get()
                                .createBottomSheetSigninIntentOrShowError(
                                        mContext,
                                        originalProfile,
                                        config,
                                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                if (intent != null) {
                    mContext.startActivity(intent);
                }
            }
        } else {
            SettingsNavigation settingsNavigation =
                    SettingsNavigationFactory.createSettingsNavigation();
            settingsNavigation.startSettings(mContext);
            SigninSurveyController.registerTrigger(
                    originalProfile,
                    SigninSurveyController.SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP);
        }
    }

    @VisibleForTesting
    boolean isProfileDataCacheEmpty() {
        return mProfileDataCache == null;
    }

    private void initializeSigninCoordinator() {
        assert mProfile != null;
        assert !mProfile.isOffTheRecord();

        if (mSigninCoordinator == null
                && SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
                && SigninFeatureMap.isEnabled(
                        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT)) {
            OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
            profileSupplier.set(mProfile);

            mSigninCoordinator =
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                    mWindowAndroid,
                                    mActivity,
                                    mActivityResultTracker,
                                    this,
                                    mDeviceLockActivityLauncher,
                                    profileSupplier,
                                    () -> mBottomSheetController,
                                    mModalDialogManagerSupplier,
                                    mSnackbarManager,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
        }
    }

    @Nullable BottomSheetSigninAndHistorySyncCoordinator getSigninCoordinatorForTesting() {
        return mSigninCoordinator;
    }
}
