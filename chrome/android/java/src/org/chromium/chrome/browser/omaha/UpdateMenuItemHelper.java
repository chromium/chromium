// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.TargetApi;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.StatFs;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.animation.LinearInterpolator;
import android.widget.TextView;

import com.google.android.gms.common.GooglePlayServicesUtil;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.appmenu.AppMenu;
import org.chromium.chrome.browser.appmenu.AppMenuItemIcon;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains logic related to displaying app menu badge and a special menu item for information
 * related to updates.
 *
 * It supports displaying a badge and item for whether an update is available, and a different
 * badge and menu item if the Android OS version Chrome is currently running on is unsupported.
 *
 * It also has logic for logging usage of the update menu item to UMA.
 *
 * For manually testing this functionality, use the following switches:
 * - {@link ChromeSwitches#FORCE_UPDATE_MENU_UPDATE_TYPE} (required)
 * - {@link ChromeSwitches#FORCE_SHOW_UPDATE_MENU_BADGE} (optional)
 * - {@link ChromeSwitches#MARKET_URL_FOR_TESTING} (optional)
 */
public class UpdateMenuItemHelper {
    @IntDef({UpdateMenuItemHelper.UpdateType.UNKNOWN, UpdateMenuItemHelper.UpdateType.NONE,
            UpdateMenuItemHelper.UpdateType.UPDATE_AVAILABLE,
            UpdateMenuItemHelper.UpdateType.UNSUPPORTED_OS_VERSION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateType {
        int UNKNOWN = 0;
        int NONE = 1;
        int UPDATE_AVAILABLE = 2;
        int UNSUPPORTED_OS_VERSION = 3;
    }

    private static final String NONE_SWITCH_VALUE = "none";
    private static final String UPDATE_AVAILABLE_SWITCH_VALUE = "update_available";
    private static final String UNSUPPORTED_OS_VERSION_SWITCH_VALUE = "unsupported_os_version";

    private static final String TAG = "UpdateMenuItemHelper";
    // VariationsAssociatedData configs
    private static final String FIELD_TRIAL_NAME = "UpdateMenuItem";
    private static final String ENABLED_VALUE = "true";
    private static final String CUSTOM_SUMMARY = "custom_summary";

    private static final String MIN_REQUIRED_STORAGE_MB = "min_required_storage_for_update_mb";
    // UMA constants for logging whether the menu item was clicked.
    private static final int ITEM_NOT_CLICKED = 0;
    private static final int ITEM_CLICKED_INTENT_LAUNCHED = 1;
    private static final int ITEM_CLICKED_INTENT_FAILED = 2;

    private static final int ITEM_CLICKED_BOUNDARY = 3;
    // UMA constants for logging whether Chrome was updated after the menu item was clicked.
    private static final int UPDATED = 0;
    private static final int NOT_UPDATED = 1;

    private static final int UPDATED_BOUNDARY = 2;
    private static UpdateMenuItemHelper sInstance;

    private static Object sGetInstanceLock = new Object();

    // Whether OmahaClient has already been checked for an update.
    private boolean mAlreadyCheckedForUpdates;

    // The current state of whether an update is available or whether it ever will be
    // (unsupported OS).
    private @UpdateType int mUpdateType;

    // URL to direct the user to when Omaha detects a newer version available.
    private String mUpdateUrl;

    // Whether the menu item was clicked. This is used to log the click-through rate.
    private boolean mMenuItemClicked;

    // The latest Chrome version available if OmahaClient.isNewerVersionAvailable() returns true.
    private String mLatestVersion;

    // If the current OS version is unsupported, and we show the menu badge, and then the user
    // clicks the badge and sees the unsupported message, we store the current version to a
    // preference and cache it here. This preference is read on startup to ensure we only show
    // the unsupported message once per version.
    private String mLatestUnsupportedVersionPreference;

    /**
     * @return The {@link UpdateMenuItemHelper} instance.
     */
    public static UpdateMenuItemHelper getInstance() {
        synchronized (UpdateMenuItemHelper.sGetInstanceLock) {
            if (sInstance == null) {
                sInstance = new UpdateMenuItemHelper();
            }
            return sInstance;
        }
    }

    /**
     * Decorates a menu item with the appropriate styling depending on the current update type.
     *
     * @param context The current context.
     * @param title The title view.
     * @param image The image view.
     * @param summary The summary view.
     */
    public void decorateMenuItemViews(
            Context context, TextView title, AppMenuItemIcon image, TextView summary) {
        switch (getUpdateType()) {
            case UpdateType.UPDATE_AVAILABLE:
                title.setText(context.getString(R.string.menu_update));
                title.setTextColor(Color.RED);

                String customSummary = getStringParamValue(CUSTOM_SUMMARY);
                if (TextUtils.isEmpty(customSummary)) {
                    summary.setText(
                            context.getResources().getString(R.string.menu_update_summary_default));
                } else {
                    summary.setText(customSummary);
                }

                image.setImageResource(R.drawable.badge_update_dark);
                break;
            case UpdateType.UNSUPPORTED_OS_VERSION:
                title.setText(R.string.menu_update_unsupported);
                title.setTextColor(ApiCompatibilityUtils.getColor(
                        context.getResources(), R.color.default_text_color));

                summary.setText(R.string.menu_update_unsupported_summary_default);

                image.setImageResource(R.drawable.ic_error_grey800_24dp_filled);
                break;
            case UpdateType.NONE:
            // Intentional fall through.
            case UpdateType.UNKNOWN:
            // Intentional fall through.
            default:
                break;
        }
    }

    /**
     * @param resources The resources to use for lookup.
     * @return The dark drawable for the badge.
     */
    @Nullable
    public Drawable getDarkBadgeDrawable(Resources resources) {
        switch (getUpdateType()) {
            case UpdateType.UPDATE_AVAILABLE:
                return ApiCompatibilityUtils.getDrawable(resources, R.drawable.badge_update_dark);
            case UpdateType.UNSUPPORTED_OS_VERSION:
                return ApiCompatibilityUtils.getDrawable(
                        resources, R.drawable.ic_error_grey800_24dp_filled);
            case UpdateType.NONE:
            // Intentional fall through.
            case UpdateType.UNKNOWN:
            // Intentional fall through.
            default:
                return null;
        }
    }

    /**
     * @param resources The resources to use for lookup.
     * @return The light drawable for the badge.
     */
    @Nullable
    public Drawable getLightBadgeDrawable(Resources resources) {
        switch (getUpdateType()) {
            case UpdateType.UPDATE_AVAILABLE:
                return ApiCompatibilityUtils.getDrawable(resources, R.drawable.badge_update_light);
            case UpdateType.UNSUPPORTED_OS_VERSION:
                return ApiCompatibilityUtils.getDrawable(
                        resources, R.drawable.ic_error_white_24dp_filled);
            case UpdateType.NONE:
            // Intentional fall through.
            case UpdateType.UNKNOWN:
            // Intentional fall through.
            default:
                return null;
        }
    }

    /**
     * Checks if the {@link OmahaClient} knows about an update.
     * @param activity The current {@link ChromeActivity}.
     */
    public void checkForUpdateOnBackgroundThread(final ChromeActivity activity) {
        ThreadUtils.assertOnUiThread();

        if (mAlreadyCheckedForUpdates) {
            if (activity.isActivityDestroyed()) return;
            activity.onCheckForUpdate();
            return;
        }

        mAlreadyCheckedForUpdates = true;

        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                if (setForcedUpdateData()) return null;

                if (VersionNumberGetter.isNewerVersionAvailable(activity)) {
                    mUpdateUrl = MarketURLGetter.getMarketUrl(activity);
                    mLatestVersion =
                            VersionNumberGetter.getInstance().getLatestKnownVersion(activity);
                    boolean hasSufficientStorage = checkForSufficientStorage();
                    mUpdateType =
                            hasSufficientStorage ? UpdateType.UPDATE_AVAILABLE : UpdateType.NONE;
                    // If a new version is available, we should later possibly show the OS not
                    // supported badge, so we need to clear the preference for now.
                    ChromePreferenceManager.getInstance().removeKey(
                            ChromePreferenceManager.LATEST_UNSUPPORTED_VERSION);
                    return null;
                }

                if (!VersionNumberGetter.isCurrentOsVersionSupported()) {
                    mUpdateType = UpdateType.UNSUPPORTED_OS_VERSION;
                    mLatestUnsupportedVersionPreference =
                            ChromePreferenceManager.getInstance().readString(
                                    ChromePreferenceManager.LATEST_UNSUPPORTED_VERSION, null);
                    return null;
                }

                mUpdateType = UpdateType.NONE;
                return null;
            }

            /**
             * @return true if all the update related data should be forced to specific values.
             */
            private boolean setForcedUpdateData() {
                String forcedUpdateType =
                        getStringParamValue(ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE);
                if (TextUtils.isEmpty(forcedUpdateType)) return false;

                switch (forcedUpdateType) {
                    case NONE_SWITCH_VALUE:
                        mUpdateType = UpdateType.NONE;
                        break;
                    case UPDATE_AVAILABLE_SWITCH_VALUE:
                        mUpdateType = UpdateType.UPDATE_AVAILABLE;
                        String testMarketUrl =
                                getStringParamValue(ChromeSwitches.MARKET_URL_FOR_TESTING);
                        if (!TextUtils.isEmpty(testMarketUrl)) mUpdateUrl = testMarketUrl;
                        break;
                    case UNSUPPORTED_OS_VERSION_SWITCH_VALUE:
                        mUpdateType = UpdateType.UNSUPPORTED_OS_VERSION;
                        // Even in the forced case, ensure that it is possible to read and write
                        // the pref, since the FORCE_SHOW_UPDATE_MENU_BADGE might not be set.
                        mLatestUnsupportedVersionPreference =
                                ChromePreferenceManager.getInstance().readString(
                                        ChromePreferenceManager.LATEST_UNSUPPORTED_VERSION, null);
                        break;
                    default:
                        // If the forced parameter or variation is set, but invalid, we should still
                        // early out of the calculation. This enables testing of the no-op state.
                        mUpdateType = UpdateType.UNKNOWN;
                        break;
                }
                return true;
            }

            @Override
            protected void onPostExecute(Void result) {
                if (activity.isActivityDestroyed()) return;
                activity.onCheckForUpdate();
                recordUpdateHistogram();
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Logs whether an update was performed if the update menu item was clicked.
     * Should be called from ChromeActivity#onStart().
     */
    public void onStart() {
        if (mAlreadyCheckedForUpdates) {
            recordUpdateHistogram();
        }
    }

    /**
     * @param context The current context.
     * @return Whether the update menu item should be shown.
     */
    public boolean shouldShowMenuItem(Context context) {
        switch (getUpdateType()) {
            case UpdateType.UPDATE_AVAILABLE:
                return isGooglePlayStoreAvailable(context);
            case UpdateType.UNSUPPORTED_OS_VERSION:
                return true;
            case UpdateType.NONE:
            // Intentional fall through.
            case UpdateType.UNKNOWN:
            // Intentional fall through.
            default:
                return false;
        }
    }

    private static boolean isGooglePlayStoreAvailable(Context context) {
        try {
            context.getPackageManager().getPackageInfo(
                    GooglePlayServicesUtil.GOOGLE_PLAY_STORE_PACKAGE, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        return true;
    }

    /**
     * @param context The current context.
     * @return Whether the update badge should be shown in the toolbar.
     */
    public boolean shouldShowToolbarBadge(Context context) {
        if (getBooleanParam(ChromeSwitches.FORCE_SHOW_UPDATE_MENU_BADGE)) return true;

        switch (getUpdateType()) {
            case UpdateType.UPDATE_AVAILABLE:
                if (!isGooglePlayStoreAvailable(context)) return false;
                // The badge is hidden if the update menu item has been clicked until there is an
                // even newer version of Chrome available.
                String latestVersionWhenClicked =
                        PrefServiceBridge.getInstance().getLatestVersionWhenClickedUpdateMenuItem();
                return !TextUtils.equals(latestVersionWhenClicked, mLatestVersion);
            case UpdateType.UNSUPPORTED_OS_VERSION:
                // We should show the badge if the user has not opened the menu.
                if (mLatestUnsupportedVersionPreference == null) return true;

                // In case the user has been upgraded since last time they tapped the toolbar badge
                // we should show the badge again.
                String currentlyUsedVersion = BuildInfo.getInstance().versionName;
                return !TextUtils.equals(mLatestUnsupportedVersionPreference, currentlyUsedVersion);
            case UpdateType.NONE: // Intentional fall through.
            case UpdateType.UNKNOWN: // Intentional fall through.
            default:
                return false;
        }
    }

    /**
     * Handles a click on the update menu item.
     * @param activity The current {@link ChromeActivity}.
     */
    public void onMenuItemClicked(ChromeActivity activity) {
        if (mUpdateType != UpdateType.UPDATE_AVAILABLE) return;
        if (mUpdateUrl == null) return;

        // If the update menu item is showing because it was forced on through about://flags
        // then mLatestVersion may be null.
        if (mLatestVersion != null) {
            PrefServiceBridge.getInstance().setLatestVersionWhenClickedUpdateMenuItem(
                    mLatestVersion);
        }

        // Fire an intent to open the URL.
        try {
            Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(mUpdateUrl));
            activity.startActivity(launchIntent);
            recordItemClickedHistogram(ITEM_CLICKED_INTENT_LAUNCHED);
            PrefServiceBridge.getInstance().setClickedUpdateMenuItem(true);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Failed to launch Activity for: %s", mUpdateUrl);
            recordItemClickedHistogram(ITEM_CLICKED_INTENT_FAILED);
        }
    }

    /**
     * Should be called before the AppMenu is dismissed if the update menu item was clicked.
     */
    public void setMenuItemClicked() {
        mMenuItemClicked = true;
    }

    /**
     * Called when the {@link AppMenu} is dimissed. Logs a histogram immediately if the update menu
     * item was not clicked. If it was clicked, logging is delayed until #onMenuItemClicked().
     */
    public void onMenuDismissed() {
        if (!mMenuItemClicked) {
            recordItemClickedHistogram(ITEM_NOT_CLICKED);
        }
        mMenuItemClicked = false;
    }

    /**
     * Called when the user clicks the app menu button while the unsupported OS badge is showing.
     */
    public void onMenuButtonClicked() {
        if (mUpdateType != UpdateType.UNSUPPORTED_OS_VERSION) return;

        // If we have already stored the current version to a preference, no need to store it again,
        // unless their Chrome version has changed.
        String currentlyUsedVersion = BuildInfo.getInstance().versionName;
        if (mLatestUnsupportedVersionPreference != null
                && mLatestUnsupportedVersionPreference.equals(currentlyUsedVersion)) {
            return;
        }

        ChromePreferenceManager.getInstance().writeString(
                ChromePreferenceManager.LATEST_UNSUPPORTED_VERSION, currentlyUsedVersion);
        mLatestUnsupportedVersionPreference = currentlyUsedVersion;
    }

    /**
     * Creates an {@link AnimatorSet} for showing the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when showing the update badge.
     */
    public static AnimatorSet createShowUpdateBadgeAnimation(final View menuButton,
            final View menuBadge) {
        // Create badge ObjectAnimators.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 1.f);
        badgeFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        int pixelTranslation = menuBadge.getResources().getDimensionPixelSize(
                R.dimen.menu_badge_translation_y_distance);
        ObjectAnimator badgeTranslateYAnimator = ObjectAnimator.ofFloat(menuBadge,
                View.TRANSLATION_Y, pixelTranslation, 0.f);
        badgeTranslateYAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 0.f);
        menuButtonFadeAnimator.setInterpolator(new LinearInterpolator());

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, badgeTranslateYAnimator, menuButtonFadeAnimator);
        set.setDuration(350);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                // Make sure the menu button is visible again.
                menuButton.setAlpha(1.f);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                // Jump to the end state if the animation is canceled.
                menuBadge.setAlpha(1.f);
                menuBadge.setTranslationY(0.f);
                menuButton.setAlpha(1.f);
            }
        });

        return set;
    }

    /**
     * Creates an {@link AnimatorSet} for hiding the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when hiding the update badge.
     */
    public static AnimatorSet createHideUpdateBadgeAnimation(final View menuButton,
            final View menuBadge) {
        // Create badge ObjectAnimator.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 0.f);
        badgeFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 1.f);
        menuButtonFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, menuButtonFadeAnimator);
        set.setDuration(200);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                menuBadge.setVisibility(View.GONE);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                // Jump to the end state if the animation is canceled.
                menuButton.setAlpha(1.f);
                menuBadge.setVisibility(View.GONE);
            }
        });

        return set;
    }

    /**
     * @return The current {@link UpdateType}. Will be {@link UpdateType#UNKNOWN} until it has been
     *         fetched on a background thread.
     */
    public @UpdateType int getUpdateType() {
        return mUpdateType;
    }

    private void recordItemClickedHistogram(int action) {
        RecordHistogram.recordEnumeratedHistogram("GoogleUpdate.MenuItem.ActionTakenOnMenuOpen",
                action, ITEM_CLICKED_BOUNDARY);
    }

    private void recordUpdateHistogram() {
        if (PrefServiceBridge.getInstance().getClickedUpdateMenuItem()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "GoogleUpdate.MenuItem.ActionTakenAfterItemClicked",
                    mUpdateType == UpdateType.UPDATE_AVAILABLE ? NOT_UPDATED : UPDATED,
                    UPDATED_BOUNDARY);
            PrefServiceBridge.getInstance().setClickedUpdateMenuItem(false);
        }
    }

    /**
     * Gets a boolean VariationsAssociatedData parameter, assuming the <paramName>="true" format.
     * Also checks for a command-line switch with the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return Whether the param is defined with a value "true", if there's a command-line
     *         flag present with any value.
     */
    private static boolean getBooleanParam(String paramName) {
        if (CommandLine.getInstance().hasSwitch(paramName)) {
            return true;
        }
        return TextUtils.equals(ENABLED_VALUE,
                VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName));
    }

    /**
     * Gets a String VariationsAssociatedData parameter. Also checks for a command-line switch with
     * the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return The command-line flag value if present, or the param is value if present.
     */
    @Nullable
    private static String getStringParamValue(String paramName) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        return value;
    }

    /**
     * Returns an integer value for a Finch parameter, or the default value if no parameter exists
     * in the current configuration.  Also checks for a command-line switch with the same name.
     * @param paramName The name of the Finch parameter (or command-line switch) to get a value for.
     * @param defaultValue The default value to return when there's no param or switch.
     * @return An integer value -- either the param or the default.
     */
    private static int getIntParamValueOrDefault(String paramName, int defaultValue) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        if (TextUtils.isEmpty(value)) return defaultValue;

        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return defaultValue;
        }
    }

    private boolean checkForSufficientStorage() {
        assert !ThreadUtils.runningOnUiThread();

        File path = Environment.getDataDirectory();
        StatFs statFs = new StatFs(path.getAbsolutePath());
        long size;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            size = getSize(statFs);
        } else {
            size = getSizeUpdatedApi(statFs);
        }
        RecordHistogram.recordLinearCountHistogram(
                "GoogleUpdate.InfoBar.InternalStorageSizeAvailable", (int) size, 1, 200, 100);
        RecordHistogram.recordLinearCountHistogram(
                "GoogleUpdate.InfoBar.DeviceFreeSpace", (int) size, 1, 1000, 50);

        int minRequiredStorage = getIntParamValueOrDefault(MIN_REQUIRED_STORAGE_MB, -1);
        if (minRequiredStorage == -1) return true;

        return size >= minRequiredStorage;
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static long getSizeUpdatedApi(StatFs statFs) {
        return ConversionUtils.bytesToMegabytes(statFs.getAvailableBytes());
    }

    @SuppressWarnings("deprecation")
    private static long getSize(StatFs statFs) {
        int blockSize = statFs.getBlockSize();
        int availableBlocks = statFs.getAvailableBlocks();
        return ConversionUtils.bytesToMegabytes(blockSize * availableBlocks);
    }
}
