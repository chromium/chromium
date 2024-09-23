// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.Choreographer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonState;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Contains logic related to displaying app menu badge and a special menu item for information
 * related to updates.
 *
 * It supports displaying a badge and item for whether an update is available, and a different
 * badge and menu item if the Android OS version Chrome is currently running on is unsupported.
 *
 * It also has logic for logging usage of the update menu item to UMA.
 *
 * For manually testing this functionality, see {@link UpdateConfigs}.
 */
public class UpdateMenuItemHelper {
    private static final String TAG = "UpdateMenuItemHelper";

    private static UpdateMenuItemHelper sInstanceForTesting;
    private static ProfileKeyedMap<UpdateMenuItemHelper> sProfileMap;

    private static Object sGetInstanceLock = new Object();

    private final Profile mProfile;
    private final ObserverList<Runnable> mObservers = new ObserverList<>();

    private final Callback<UpdateStatusProvider.UpdateStatus> mUpdateCallback =
            status -> {
                mStatus = status;
                handleStateChanged();
                pingObservers();
            };

    /**
     * The current state of updates for Chrome. This can change during runtime and may be {@code
     * null} if the status hasn't been determined yet.
     *
     * <p>TODO(crbug.com/40610457): Handle state bug where the state here and the visible state of
     * the UI can be out of sync.
     */
    private @Nullable UpdateStatus mStatus;

    private @NonNull MenuUiState mMenuUiState = new MenuUiState();

    /**
     * Whether the runnable posted when the app menu is dismissed has been executed. Tracked for
     * testing.
     */
    private boolean mMenuDismissedRunnableExecuted;

    /** Return the {@link UpdateMenuItemHelper} for the given {@link Profile}. */
    public static UpdateMenuItemHelper getInstance(Profile profile) {
        synchronized (UpdateMenuItemHelper.sGetInstanceLock) {
            if (sInstanceForTesting != null) return sInstanceForTesting;
            if (sProfileMap == null) {
                sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
            }
            return sProfileMap.getForProfile(profile, UpdateMenuItemHelper::new);
        }
    }

    public static void setInstanceForTesting(UpdateMenuItemHelper testingInstance) {
        sInstanceForTesting = testingInstance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private UpdateMenuItemHelper(Profile profile) {
        mProfile = profile;
    }

    /**
     * Registers {@code observer} to be triggered whenever the menu state changes.  This will always
     * be triggered at least once after registration.
     */
    public void registerObserver(Runnable observer) {
        if (!mObservers.addObserver(observer)) return;

        if (mStatus != null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (mObservers.hasObserver(observer)) observer.run();
                    });
            return;
        }

        UpdateStatusProvider.getInstance().addObserver(mUpdateCallback);
    }

    /** Unregisters {@code observer} from menu state changes. */
    public void unregisterObserver(Runnable observer) {
        mObservers.removeObserver(observer);
    }

    /** @return {@link MenuUiState} representing the current update state for the menu. */
    public @NonNull MenuUiState getUiState() {
        return mMenuUiState;
    }

    /**
     * Handles a click on the update menu item.
     * @param activity The current {@code Activity}.
     */
    public void onMenuItemClicked(Activity activity) {
        if (mStatus == null) return;

        switch (mStatus.updateState) {
            case UpdateState.UPDATE_AVAILABLE:
                if (TextUtils.isEmpty(mStatus.updateUrl)) return;

                try {
                    UpdateStatusProvider.getInstance()
                            .startIntentUpdate(activity, /* newTask= */ false);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "Failed to launch Activity for: %s", mStatus.updateUrl);
                }
                break;
            case UpdateState.UNSUPPORTED_OS_VERSION:
                // Intentional fall through.
            default:
                return;
        }

        // If the update menu item is showing because it was forced on through about://flags
        // then mLatestVersion may be null.
        if (mStatus.latestVersion != null) {
            getPrefService()
                    .setString(
                            Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM,
                            mStatus.latestVersion);
        }

        handleStateChanged();
    }

    /** Called when the menu containing the update menu item is dismissed. */
    public void onMenuDismissed() {
        mMenuDismissedRunnableExecuted = false;
        // Post a task to record the item clicked histogram. Post task is used so that the runnable
        // executes after #onMenuItemClicked is called (if it's going to be called).
        Choreographer.getInstance()
                .postFrameCallback(
                        (long frameTimeNanos) -> {
                            mMenuDismissedRunnableExecuted = true;
                        });
    }

    /**
     * Called when the user clicks the app menu button while the unsupported OS badge is showing.
     */
    public void onMenuButtonClicked() {
        if (mStatus == null) return;
        if (mStatus.updateState != UpdateState.UNSUPPORTED_OS_VERSION) return;

        UpdateStatusProvider.getInstance().updateLatestUnsupportedVersion();
    }

    private void handleStateChanged() {
        assert mStatus != null;

        boolean showBadge = UpdateConfigs.getAlwaysShowMenuBadge();

        // Note that is not safe for theming, but for string access it should be ok.
        Resources resources = ContextUtils.getApplicationContext().getResources();

        mMenuUiState = new MenuUiState();
        switch (mStatus.updateState) {
            case UpdateState.UPDATE_AVAILABLE:
                // The badge is hidden if the update menu item has been clicked until there is an
                // even newer version of Chrome available.
                showBadge |=
                        !TextUtils.equals(
                                getPrefService()
                                        .getString(
                                                Pref.LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM),
                                mStatus.latestUnsupportedVersion);

                if (showBadge) {
                    mMenuUiState.buttonState = new MenuButtonState();
                    mMenuUiState.buttonState.menuContentDescription =
                            R.string.accessibility_toolbar_btn_menu_update;
                    mMenuUiState.buttonState.darkBadgeIcon = R.drawable.badge_update_dark;
                    mMenuUiState.buttonState.lightBadgeIcon = R.drawable.badge_update_light;
                    mMenuUiState.buttonState.adaptiveBadgeIcon = R.drawable.badge_update;
                }

                mMenuUiState.itemState = new MenuItemState();
                mMenuUiState.itemState.title = R.string.menu_update;
                mMenuUiState.itemState.titleColorId = R.color.default_text_color_error;
                mMenuUiState.itemState.icon = R.drawable.badge_update;
                mMenuUiState.itemState.enabled = true;
                mMenuUiState.itemState.summary = UpdateConfigs.getCustomSummary();
                if (TextUtils.isEmpty(mMenuUiState.itemState.summary)) {
                    mMenuUiState.itemState.summary =
                            resources.getString(R.string.menu_update_summary_default);
                }
                break;
            case UpdateState.UNSUPPORTED_OS_VERSION:
                // We should show the badge if the user has not opened the menu.
                showBadge |= mStatus.latestUnsupportedVersion == null;

                // In case the user has been upgraded since last time they tapped the toolbar badge
                // we should show the badge again.
                showBadge |=
                        !TextUtils.equals(
                                BuildInfo.getInstance().versionName,
                                mStatus.latestUnsupportedVersion);

                if (showBadge) {
                    mMenuUiState.buttonState = new MenuButtonState();
                    mMenuUiState.buttonState.menuContentDescription =
                            R.string.accessibility_toolbar_btn_menu_os_version_unsupported;
                    mMenuUiState.buttonState.darkBadgeIcon =
                            R.drawable.ic_error_grey800_24dp_filled;
                    mMenuUiState.buttonState.lightBadgeIcon = R.drawable.ic_error_white_24dp_filled;
                    mMenuUiState.buttonState.adaptiveBadgeIcon = R.drawable.ic_error_24dp_filled;
                }

                mMenuUiState.itemState = new MenuItemState();
                mMenuUiState.itemState.title = R.string.menu_update_unsupported;
                mMenuUiState.itemState.titleColorId = R.color.default_text_color_list;
                mMenuUiState.itemState.summary =
                        resources.getString(R.string.menu_update_unsupported_summary_default);
                mMenuUiState.itemState.icon = R.drawable.ic_error_24dp_filled;
                mMenuUiState.itemState.enabled = false;
                break;
            case UpdateState.NONE:
                // Intentional fall through.
            default:
                break;
        }
    }

    private void pingObservers() {
        for (Runnable observer : mObservers) observer.run();
    }

    private PrefService getPrefService() {
        return UserPrefs.get(mProfile);
    }

    boolean getMenuDismissedRunnableExecutedForTests() {
        return mMenuDismissedRunnableExecuted;
    }
}
