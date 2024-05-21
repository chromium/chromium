// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A helper activity for routing launcher shortcut intents. */
public class LauncherShortcutActivity extends Activity {
    public static final String ACTION_OPEN_NEW_TAB = "chromium.shortcut.action.OPEN_NEW_TAB";
    public static final String ACTION_OPEN_NEW_INCOGNITO_TAB =
            "chromium.shortcut.action.OPEN_NEW_INCOGNITO_TAB";

    @VisibleForTesting
    static final String DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID = "dynamic-new-incognito-tab-shortcut";

    private static String sLabelForTesting;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        String intentAction = intent.getAction();

        // Exit early if the original intent action isn't for opening a new tab.
        if (!intentAction.equals(ACTION_OPEN_NEW_TAB)
                && !intentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB)) {
            finish();
            return;
        }

        Intent newIntent = getChromeLauncherActivityIntent(this, intentAction);
        // Retain FLAG_ACTIVITY_MULTIPLE_TASK in the intent if present, to support multi-instance
        // launch behavior.
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_MULTIPLE_TASK) != 0) {
            newIntent.setFlags(newIntent.getFlags() | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        }

        startActivity(newIntent);
        finish();
    }

    /**
     * Adds or removes the "New incognito tab" launcher shortcut based on whether incognito mode is
     * enabled.
     *
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     * @param profile The profile used to check whether incognito mode is enabled.
     */
    public static void updateIncognitoShortcut(Context context, Profile profile) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1) return;

        SharedPreferencesManager preferences = ChromeSharedPreferences.getInstance();
        boolean incognitoEnabled = IncognitoUtils.isIncognitoModeEnabled(profile);
        boolean incognitoShortcutAdded =
                preferences.readBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, false);

        // Add the shortcut regardless of whether it was previously added in case the locale has
        // changed since the last addition.
        // TODO(crbug.com/40125673): Investigate better locale change handling.
        if (incognitoEnabled) {
            boolean success = LauncherShortcutActivity.addIncognitoLauncherShortcut(context);

            // Save a shared preference indicating the incognito shortcut has been added.
            if (success) {
                preferences.writeBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, true);
            }
        } else if (!incognitoEnabled && incognitoShortcutAdded) {
            LauncherShortcutActivity.removeIncognitoLauncherShortcut(context);
            preferences.writeBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, false);
        }
    }

    /**
     * Adds a "New incognito tab" dynamic launcher shortcut.
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     * @return True if adding the shortcut has succeeded. False if the call fails due to rate
     *         limiting. See {@link ShortcutManager#addDynamicShortcuts}.
     */
    @RequiresApi(Build.VERSION_CODES.N_MR1)
    private static boolean addIncognitoLauncherShortcut(Context context) {
        Intent intent = new Intent(LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB);
        intent.setPackage(context.getPackageName());
        intent.setClass(context, LauncherShortcutActivity.class);

        ShortcutInfo shortcut =
                new ShortcutInfo.Builder(context, DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID)
                        .setShortLabel(
                                context.getResources()
                                        .getString(
                                                R.string
                                                        .accessibility_tabstrip_incognito_identifier))
                        .setLongLabel(
                                sLabelForTesting != null
                                        ? sLabelForTesting
                                        : context.getResources()
                                                .getString(R.string.menu_new_incognito_tab))
                        .setIcon(Icon.createWithResource(context, R.drawable.shortcut_incognito))
                        .setIntent(intent)
                        .build();

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        return shortcutManager.addDynamicShortcuts(Arrays.asList(shortcut));
    }

    /**
     * Removes the dynamic "New incognito tab" launcher shortcut.
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     */
    @RequiresApi(Build.VERSION_CODES.N_MR1)
    private static void removeIncognitoLauncherShortcut(Context context) {
        List<String> shortcutList = new ArrayList<>();
        shortcutList.add(DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID);

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        shortcutManager.disableShortcuts(shortcutList);
        shortcutManager.removeDynamicShortcuts(shortcutList);
    }

    /**
     * @param context The context used to get the package and set the intent class.
     * @param launcherShortcutIntentAction The intent action that launched the
     *                                     LauncherShortcutActivity.
     * @return An intent for ChromeLauncherActivity that will open a new regular or incognito tab.
     */
    private static Intent getChromeLauncherActivityIntent(
            Context context, String launcherShortcutIntentAction) {
        Intent newIntent =
                IntentHandler.createTrustedOpenNewTabIntent(
                        context,
                        launcherShortcutIntentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB));
        newIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);

        return newIntent;
    }

    public static void setDynamicShortcutStringForTesting(String label) {
        sLabelForTesting = label;
        ResettersForTesting.register(() -> sLabelForTesting = null);
    }
}
