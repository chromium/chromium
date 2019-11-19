// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.ChromeSigninController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions
 * (user sign-in state, whether NTP is shown)
 */
class IdentityDiscController implements NativeInitObserver, ProfileDataCache.Observer,
                                        SigninManager.SignInStateObserver,
                                        ProfileSyncService.SyncStateChangedListener {
    // Visual state of Identity Disc.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({IdentityDiscState.NONE, IdentityDiscState.SMALL, IdentityDiscState.LARGE})
    private @interface IdentityDiscState {
        // Identity Disc is hidden.
        int NONE = 0;

        // Small Identity Disc is shown.
        int SMALL = 1;

        // Large Identity Disc is shown.
        int LARGE = 2;
        int MAX = 3;
    }

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    // Toolbar manager exposes APIs for manipulating experimental button.
    private final ToolbarManager mToolbarManager;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // SigninManager and ProfileSyncService allow observing sign-in and sync state.
    private SigninManager mSigninManager;
    private ProfileSyncService mProfileSyncService;

    // ProfileDataCache facilitates retrieving profile picture. Separate objects are maintained
    // for different visual states to cache profile pictures of different size.
    // mProfileDataCache[IdentityDiscState.NONE] should always be null since in this state
    // Identity Disc is not visible.
    private ProfileDataCache mProfileDataCache[] = new ProfileDataCache[IdentityDiscState.MAX];

    // Identity disc visibility state.
    @IdentityDiscState
    private int mState = IdentityDiscState.NONE;

    private boolean mIsNTPVisible;

    /**
     * Creates IdentityDiscController object.
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param toolbarManager The ToolbarManager where Identity Disc is displayed.
     */
    IdentityDiscController(Context context, ToolbarManager toolbarManager,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mContext = context;
        mToolbarManager = toolbarManager;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    /**
     * Registers itself to observe sign-in and sync status events.
     */
    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;

        mProfileSyncService = ProfileSyncService.get();
        // ProfileSyncService being null means sync is disabled and won't be initialized. This means
        // Identity Disc will never get shown so we don't need to register for other notifications.
        if (mProfileSyncService == null) return;

        mProfileSyncService.addSyncStateChangedListener(this);

        mSigninManager = IdentityServicesProvider.getSigninManager();
        mSigninManager.addSignInStateObserver(this);
    }

    /**
     * Shows/hides Identity Disc depending on whether NTP is visible.
     */
    void updateButtonState() {
        updateButtonState(mIsNTPVisible);
    }

    /**
     * Shows/hides Identity Disc depending on whether NTP is visible.
     */
    void updateButtonState(boolean isNTPVisible) {
        // Sync is disabled. IdentityDisc will never be shown.
        if (mProfileSyncService == null) return;

        mIsNTPVisible = isNTPVisible;
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        boolean shouldShowIdentityDisc = isNTPVisible && accountName != null
                && ProfileSyncService.get().canSyncFeatureStart();
        @IdentityDiscState
        int oldState = mState;

        mState = !shouldShowIdentityDisc
                ? IdentityDiscState.NONE
                : mToolbarManager.isBottomToolbarVisible() ? IdentityDiscState.LARGE
                                                           : IdentityDiscState.SMALL;

        if (mState != IdentityDiscState.NONE && mState != oldState) {
            // When showing Identity Disc or updating its size fetch corresponding profile picture.
            createProfileDataCache(accountName, mState);
        }

        if (mState != IdentityDiscState.NONE) {
            if (oldState == IdentityDiscState.NONE) {
                showIdentityDisc(accountName);
                maybeShowIPH();
            } else if (mState != oldState) {
                mToolbarManager.updateExperimentalButtonImage(getProfileImage(accountName));
            }
        } else if (oldState != IdentityDiscState.NONE) {
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void createProfileDataCache(String accountName, @IdentityDiscState int state) {
        assert state != IdentityDiscState.NONE;
        if (mProfileDataCache[state] != null) return;

        @DimenRes
        int dimension_id =
                (state == IdentityDiscState.SMALL) ? R.dimen.toolbar_identity_disc_size
                                                   : R.dimen.toolbar_identity_disc_size_duet;
        int imageSize = mContext.getResources().getDimensionPixelSize(dimension_id);
        ProfileDataCache profileDataCache = new ProfileDataCache(mContext, imageSize);
        profileDataCache.addObserver(this);
        profileDataCache.update(Collections.singletonList(accountName));
        mProfileDataCache[state] = profileDataCache;
    }

    /**
     * Returns Profile picture Drawable. The size of the image corresponds to current visual state.
     */
    private Drawable getProfileImage(String accountName) {
        assert mState != IdentityDiscState.NONE;
        return mProfileDataCache[mState].getProfileDataOrDefault(accountName).getImage();
    }

    /**
     * Triggers profile image fetch and displays Identity Disc on top toolbar.
     */
    private void showIdentityDisc(String accountName) {
        mToolbarManager.enableExperimentalButton(view -> {
            recordIdentityDiscUsed();
            PreferencesLauncher.launchSettingsPage(mContext, SyncAndServicesPreferences.class);
        }, getProfileImage(accountName), R.string.accessibility_toolbar_btn_identity_disc);
    }

    /**
     * Hides IdentityDisc and resets all ProfileDataCache objects. Used for flushing cached images
     * when sign-in state changes.
     */
    private void resetIdentityDisc() {
        for (int i = 0; i < IdentityDiscState.MAX; i++) {
            if (mProfileDataCache[i] != null) {
                assert i != IdentityDiscState.NONE;
                mProfileDataCache[i].removeObserver(this);
                mProfileDataCache[i] = null;
            }
        }
        if (mState != IdentityDiscState.NONE) {
            mState = IdentityDiscState.NONE;
            mToolbarManager.disableExperimentalButton();
        }
    }

    /**
     * Called after profile image becomes available. Updates the image on toolbar button.
     */
    @Override
    public void onProfileDataUpdated(String accountId) {
        if (mState == IdentityDiscState.NONE) return;
        assert mProfileDataCache[mState] != null;

        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (accountId.equals(accountName)) {
            mToolbarManager.updateExperimentalButtonImage(getProfileImage(accountName));
        }
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        resetIdentityDisc();
        updateButtonState();
    }

    @Override
    public void onSignedOut() {
        updateButtonState();
    }

    // ProfileSyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updateButtonState();
    }

    /**
     * Call to tear down dependencies.
     */
    void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        for (int i = 0; i < IdentityDiscState.MAX; i++) {
            if (mProfileDataCache[i] != null) {
                mProfileDataCache[i].removeObserver(this);
                mProfileDataCache[i] = null;
            }
        }
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
            mSigninManager = null;
        }
        if (mProfileSyncService != null) {
            mProfileSyncService.removeSyncStateChangedListener(this);
            mProfileSyncService = null;
        }
    }

    /**
     * Shows a help bubble below Identity Disc if the In-Product Help conditions are met.
     */
    private void maybeShowIPH() {
        Profile profile = Profile.getLastUsedProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.IDENTITY_DISC_FEATURE)) return;

        mToolbarManager.showIPHOnExperimentalButton(R.string.iph_identity_disc_text,
                R.string.iph_identity_disc_accessibility_text,
                () -> { tracker.dismissed(FeatureConstants.IDENTITY_DISC_FEATURE); });
    }

    /**
     * Records IdentityDisc usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help.
     */
    private void recordIdentityDiscUsed() {
        Profile profile = Profile.getLastUsedProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }
}
