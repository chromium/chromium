// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.SharedPreferences;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.util.HashSet;

/** Utilities for educational tip modules. */
@NullMarked
public class EducationalTipModuleUtils {

    /** Returns a list of module types supported by EducationalTip builder and mediator. */
    public static HashSet<Integer> getModuleTypes() {
        HashSet<Integer> modules = new HashSet<>();
        modules.add(ModuleType.DEFAULT_BROWSER_PROMO);
        modules.add(ModuleType.TAB_GROUP_PROMO);
        modules.add(ModuleType.TAB_GROUP_SYNC_PROMO);
        modules.add(ModuleType.QUICK_DELETE_PROMO);
        modules.add(ModuleType.HISTORY_SYNC_PROMO);
        return modules;
    }

    /**
     * Records whether the default browser promo is allowed to be displayed on relaunch. This state
     * is determined when the activity is paused and is used to decide whether to show the promo
     * after the app restarts.
     *
     * @param profileSupplier An observable supplier for the user's profile.
     */
    public static void setDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreference(
            ObservableSupplier<Profile> profileSupplier) {
        Profile profile = profileSupplier.get();
        assert profile != null;
        profile = profile.getOriginalProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);

        setDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreferenceImpl(
                tracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK));
    }

    /**
     * Sets a shared preference to indicate whether the default browser promo should be displayed on
     * the next relaunch.
     *
     * @param wouldTrigger A boolean indicating if the promo would be triggered.
     */
    private static void setDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreferenceImpl(
            boolean wouldTrigger) {
        SharedPreferences.Editor editor = ChromeSharedPreferences.getInstance().getEditor();
        editor.putBoolean(
                ChromePreferenceKeys
                        .EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_ALLOW_DISPLAY_FOR_RELAUNCH,
                wouldTrigger);
        // The ChromeSharedPreferences.getInstance().writeInt() method uses editor.apply() instead
        // of editor.commit(). The editor.apply() method writes data to memory and returns
        // immediately, while the actual disk write occurs asynchronously in a background thread. On
        // the other hand, editor.commit() writes data directly to disk and waits for the operation
        // to complete. Since apply() is asynchronous, if the program is forcibly closed right after
        // calling it (e.g., in our case where Chrome is closed and then relaunched), the disk write
        // may not finish in time, potentially resulting in data loss. Therefore, editor.commit() is
        // used here to ensure data is reliably saved.
        editor.commit();
    }

    /**
     * Retrieves the stored preference for whether to allow the display of the default browser promo
     * upon relaunch.
     */
    static boolean getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readBoolean(
                ChromePreferenceKeys
                        .EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_ALLOW_DISPLAY_FOR_RELAUNCH,
                false);
    }
}
