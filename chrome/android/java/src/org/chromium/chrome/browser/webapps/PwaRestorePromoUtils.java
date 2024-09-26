// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.jni_zero.JNINamespace;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreBottomSheetCoordinator;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * This class is responsible for coordinating the showing of the PWA Restore promo (which aims to
 * remind users that they had PWAs installed on their old device, and can restore them on their new
 * device.
 */
@JNINamespace("webapps")
public class PwaRestorePromoUtils {
    private static final String TAG = "PwaRestore";

    /** Used to determine at what stage in the display process this promo is. */
    @IntDef({
        DisplayStage.UNKNOWN_STATUS,
        DisplayStage.SHOW_PROMO,
        DisplayStage.ALREADY_LAUNCHED,
        DisplayStage.NO_APPS_AVAILABLE,
        DisplayStage.PRE_EXISTING_PROFILE,
        DisplayStage.ERROR_LAUNCHING_PROMO,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DisplayStage {
        int UNKNOWN_STATUS = 0;
        int SHOW_PROMO = 1;
        int ALREADY_LAUNCHED = 2;
        int NO_APPS_AVAILABLE = 3;
        int PRE_EXISTING_PROFILE = 4;
        int ERROR_LAUNCHING_PROMO = 5;
    }

    public static void notifyFirstRunPromoTriggered() {
        Log.i(TAG, "First Run Promo has been triggered, setting flag to show on next launch.");
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        // Promos are not shown alongside the First Run Experience, so write a flag to trigger the
        // promo to show during next launch.
        preferenceManager.writeInt(
                ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.SHOW_PROMO);
    }

    public static boolean maybeForceShowPromo(Profile profile, WindowAndroid windowAndroid) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_RESTORE_UI_AT_STARTUP)) {
            Log.i(TAG, "Force showing PWA Restore promo at startup, feature flag is enabled.");
            launchPromo(profile, windowAndroid);
            return true;
        }
        return false;
    }

    /**
     * Launch the PWA Restore promotion, if we've determined that this launch meets the criteria for
     * for showing it. The promo is intended to show as soon as possible after the user has switched
     * to a new device. It keeps track of when the first-run experience has been triggered (see
     * `notifyFirstRunPromoTriggered`) and launches when the flag is seen, which normally is the
     * next launch _after_ the first run experience launch.
     *
     * @param windowAndroid The current {@link WindowAndroid} to use for this promo.
     * @return Whether the PWA Restore promo was shown.
     */
    public static boolean launchPromoIfNeeded(Profile profile, WindowAndroid windowAndroid) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_RESTORE_UI)) {
            Log.i(TAG, "Not showing PWA Restore promo, feature flag is disabled.");
            return false;
        }

        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        // TODO(finnur): Change the default to be false when the backend writes this pref.
        if (!preferenceManager.readBoolean(ChromePreferenceKeys.PWA_RESTORE_APPS_AVAILABLE, true)) {
            Log.i(TAG, "Not showing PWA Restore promo, no apps available for restoring.");
            preferenceManager.writeInt(
                    ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.NO_APPS_AVAILABLE);
            return false;
        }

        @DisplayStage
        int promoStage =
                preferenceManager.readInt(
                        ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.UNKNOWN_STATUS);
        Log.i(TAG, "Currently saved promo stage: " + promoStage + ".");

        switch (promoStage) {
            case DisplayStage.UNKNOWN_STATUS:
                // For most users, the current launch is not the initial or second startup, and
                // therefore nothing has been written as the DisplayStage for this promo (because
                // `notifyFirstRunPromoTriggered` has not been called). The promo will therefore
                // write PRE_EXISTING_PROFILE to the DisplayStage to prevent the promo from showing
                // now and in the future.
                Log.i(TAG, "Promo stage is unknown - marking as existing profile.");
                preferenceManager.writeInt(
                        ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE,
                        DisplayStage.PRE_EXISTING_PROFILE);
                return false;
            case DisplayStage.PRE_EXISTING_PROFILE:
                // This prevents the promo from showing in the future.
                Log.i(TAG, "Not showing promo - old profile.");
                return false;
            case DisplayStage.ALREADY_LAUNCHED:
                // If the promo has already launched, our work is done.
                Log.i(TAG, "Not showing promo - promo has already launched.");
                return false;
            case DisplayStage.NO_APPS_AVAILABLE:
                // If the promo has already launched, our work is done.
                Log.i(TAG, "Not showing promo - no apps available.");
                return false;
            case DisplayStage.ERROR_LAUNCHING_PROMO:
                // Last time we launched there was an error. For now, don't launch again.
                Log.i(TAG, "Not showing promo - prior error.");
                return false;
            case DisplayStage.SHOW_PROMO:
                // We've determined that the promo should show. If successfully shown, we'll mark
                // it as such, to prevent the promo from appearing in the future.
                launchPromo(profile, windowAndroid);
                return true;
            default:
                assert false;
                return false;
        }
    }

    private static void launchPromo(Profile profile, WindowAndroid windowAndroid) {
        WebApkSyncService.fetchRestorableApps(
                profile,
                (success, appIds, names, lastUsedInDays, icons) -> {
                    onRestorableAppsAvailable(
                            success,
                            appIds,
                            names,
                            lastUsedInDays,
                            icons,
                            windowAndroid,
                            R.drawable.ic_arrow_back_24dp);
                });
    }

    private static void onRestorableAppsAvailable(
            boolean success,
            @NonNull String[] appIds,
            @NonNull String[] appNames,
            @NonNull int[] lastUsedInDays,
            @NonNull List<Bitmap> icons,
            WindowAndroid windowAndroid,
            int arrowResourceId) {
        BottomSheetController controller = BottomSheetControllerProvider.from(windowAndroid);
        if (controller == null) {
            success = false;
        } else {
            Activity activity = windowAndroid.getActivity().get();
            PwaRestoreBottomSheetCoordinator pwaRestoreBottomSheetCoordinator =
                    new PwaRestoreBottomSheetCoordinator(
                            appIds,
                            appNames,
                            icons,
                            lastUsedInDays,
                            activity,
                            controller,
                            arrowResourceId);
            if (pwaRestoreBottomSheetCoordinator == null
                    || !pwaRestoreBottomSheetCoordinator.show()) {
                success = false;
            }
        }

        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        if (success) {
            preferenceManager.writeInt(
                    ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE, DisplayStage.ALREADY_LAUNCHED);
        } else {
            preferenceManager.writeInt(
                    ChromePreferenceKeys.PWA_RESTORE_PROMO_STAGE,
                    DisplayStage.ERROR_LAUNCHING_PROMO);
        }
      }
}
