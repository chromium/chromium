// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.drawable.Icon;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A helper activity for routing launcher shortcut intents. */
@NullMarked
public class LauncherShortcutActivity extends Activity {
    public static final String ACTION_OPEN_NEW_TAB = "chromium.shortcut.action.OPEN_NEW_TAB";
    public static final String ACTION_OPEN_NEW_WINDOW = "chromium.shortcut.action.OPEN_NEW_WINDOW";
    public static final String ACTION_OPEN_NEW_INCOGNITO_TAB =
            "chromium.shortcut.action.OPEN_NEW_INCOGNITO_TAB";
    public static final String ACTION_OPEN_NEW_INCOGNITO_WINDOW =
            "chromium.shortcut.action.OPEN_NEW_INCOGNITO_WINDOW";

    @VisibleForTesting
    static final String DYNAMIC_OPEN_NEW_WINDOW_ID = "dynamic-new-window-shortcut";

    static final String DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID = "dynamic-new-incognito-tab-shortcut";

    private static @Nullable String sLabelForTesting;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        String intentAction = intent.getAction();
        assumeNonNull(intentAction);

        // Exit early if the original intent action isn't for opening a new tab.
        if (!intentAction.equals(ACTION_OPEN_NEW_TAB)
                && !intentAction.equals(ACTION_OPEN_NEW_WINDOW)
                && !intentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB)
                && !intentAction.equals(ACTION_OPEN_NEW_INCOGNITO_WINDOW)) {
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
        SharedPreferencesManager preferences = ChromeSharedPreferences.getInstance();
        boolean incognitoEnabled = IncognitoUtils.isIncognitoModeEnabled(profile);
        boolean incognitoShortcutAdded =
                preferences.readBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, false);

        // Add the shortcut regardless of whether it was previously added in case the locale has
        // changed since the last addition.
        // TODO(crbug.com/40125673): Investigate better locale change handling.
        if (incognitoEnabled) {
            boolean success = LauncherShortcutActivity.addExtraLauncherShortcut(context);

            // Save a shared preference indicating the incognito shortcut has been added.
            if (success) {
                preferences.writeBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, true);
            }
        } else if (incognitoShortcutAdded) {
            LauncherShortcutActivity.removeLauncherShortcuts(context);
            preferences.writeBoolean(ChromePreferenceKeys.INCOGNITO_SHORTCUT_ADDED, false);
        }
    }

    /**
     * Adds a "New incognito tab" or "New window" and "New incognito window" dynamic launcher
     * shortcut based on whether mixed windows are supported.
     *
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     * @return True if adding the shortcut has succeeded. False if the call fails due to rate
     *     limiting. See {@link ShortcutManager#addDynamicShortcuts}.
     */
    private static boolean addExtraLauncherShortcut(Context context) {
        boolean supportedMixedWindows = !IncognitoUtils.shouldOpenIncognitoAsWindow();
        if (supportedMixedWindows) {
            return addLauncherShortcut(
                    context,
                    DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID,
                    LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB,
                    R.string.accessibility_incognito_tab,
                    R.string.menu_new_incognito_tab,
                    R.drawable.shortcut_incognito);
        } else {
            boolean newWindowSuccess =
                    addLauncherShortcut(
                            context,
                            DYNAMIC_OPEN_NEW_WINDOW_ID,
                            LauncherShortcutActivity.ACTION_OPEN_NEW_WINDOW,
                            R.string.menu_new_window,
                            R.string.menu_new_window,
                            R.drawable.shortcut_newwindow);
            boolean newIncognitoWindowSuccess =
                    addLauncherShortcut(
                            context,
                            DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID,
                            LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_WINDOW,
                            R.string.menu_incognito_window,
                            R.string.menu_new_incognito_window,
                            R.drawable.shortcut_incognito);
            return newWindowSuccess && newIncognitoWindowSuccess;
        }
    }

    private static boolean addLauncherShortcut(
            Context context,
            String shortcutId,
            String action,
            int shortLabelResId,
            int longLabelResId,
            int iconResId) {
        Intent intent = new Intent(action);
        intent.setPackage(context.getPackageName());
        intent.setClass(context, LauncherShortcutActivity.class);

        ShortcutInfo shortcut =
                new ShortcutInfo.Builder(context, shortcutId)
                        .setShortLabel(context.getString(shortLabelResId))
                        .setLongLabel(
                                sLabelForTesting != null
                                        ? sLabelForTesting
                                        : context.getString(longLabelResId))
                        .setIcon(Icon.createWithResource(context, iconResId))
                        .setIntent(intent)
                        .build();

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        return shortcutManager.addDynamicShortcuts(Arrays.asList(shortcut));
    }

    /**
     * Removes the dynamic "New incognito tab" and "New window" launcher shortcut.
     *
     * @param context The context used to retrieve the system {@link ShortcutManager}.
     */
    private static void removeLauncherShortcuts(Context context) {
        List<String> shortcutList = new ArrayList<>();
        shortcutList.add(DYNAMIC_OPEN_NEW_INCOGNITO_TAB_ID);
        shortcutList.add(DYNAMIC_OPEN_NEW_WINDOW_ID);

        ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        shortcutManager.disableShortcuts(shortcutList);
        shortcutManager.removeDynamicShortcuts(shortcutList);
    }

    /**
     * @param context The context used to get the package and set the intent class.
     * @param launcherShortcutIntentAction The intent action that launched the
     *     LauncherShortcutActivity.
     * @return An intent for ChromeLauncherActivity that will open a new regular or incognito tab.
     */
    private static Intent getChromeLauncherActivityIntent(
            Context context, String launcherShortcutIntentAction) {
        Intent newIntent;
        if (launcherShortcutIntentAction.equals(ACTION_OPEN_NEW_TAB)
                || launcherShortcutIntentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB)) {
            newIntent =
                    IntentHandler.createTrustedOpenNewTabIntent(
                            context,
                            launcherShortcutIntentAction.equals(ACTION_OPEN_NEW_INCOGNITO_TAB));
        } else {
            newIntent =
                    IntentHandler.createTrustedOpenNewWindowIntent(
                            context,
                            launcherShortcutIntentAction.equals(ACTION_OPEN_NEW_INCOGNITO_WINDOW));
        }
        newIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);

        return newIntent;
    }

    public static void setDynamicShortcutStringForTesting(String label) {
        sLabelForTesting = label;
        ResettersForTesting.register(() -> sLabelForTesting = null);
    }
}
