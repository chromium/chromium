// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.Menu;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;

import com.google.android.material.appbar.MaterialToolbar;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/**
 * Helper class to share menu handling logic between {@link SettingsActivity} and {@link
 * SettingsPageFragmentDelegateImpl}.
 */
@NullMarked
public class SettingsMenuHelper {
    /** Delegate for handling settings menu actions. */
    public interface Delegate {
        /** Returns the current main fragment. */
        @Nullable Fragment getMainFragment();

        /** Returns the {@link MultiColumnSettings} if available. */
        @Nullable MultiColumnSettings getMultiColumnSettings();

        /** Returns the {@link SettingsSearchCoordinator} if available. */
        @Nullable SettingsSearchCoordinator getSearchCoordinator();

        /** Returns the {@link HelpAndFeedbackLauncher} to use. */
        HelpAndFeedbackLauncher getHelpAndFeedbackLauncher();

        /** Finishes the settings UI (e.g. activity). */
        void finishSettings();

        /** Handles the back button press. */
        void onBackPressed();

        /** Finishes the current settings fragment. */
        void finishCurrentSettings(Fragment fragment);
    }

    /**
     * Helper to create the options menu.
     *
     * @param menu The Menu to populate.
     * @param activity The Activity hosting the menu.
     */
    public static void onCreateOptionsMenu(Menu menu, Activity activity) {
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help =
                menu.add(
                        Menu.NONE,
                        R.id.menu_id_general_help,
                        Menu.CATEGORY_SECONDARY,
                        HelpAndFeedbackLauncher.getHelpMenuStringRes());
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        activity.getResources(), R.drawable.ic_help_24dp, activity.getTheme()));
    }

    /**
     * Helper to prepare the options menu.
     *
     * @param menu The Menu to prepare.
     */
    public static void onPrepareOptionsMenu(Menu menu) {
        if (menu.size() == 1) {
            MenuItem item = menu.getItem(0);
            if (item.getIcon() != null) {
                item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
            }
        }
    }

    /**
     * Helper to handle menu item selection.
     *
     * @param item The selected MenuItem.
     * @param activity The Activity hosting the menu.
     * @param delegate The Delegate to handle specific actions.
     * @return True if the menu item was handled.
     */
    public static boolean onOptionsItemSelected(
            MenuItem item, Activity activity, Delegate delegate) {
        Fragment mainFragment = delegate.getMainFragment();
        if (mainFragment != null && mainFragment.onOptionsItemSelected(item)) {
            if (item.getItemId() == R.id.menu_id_targeted_help) {
                RecordUserAction.record("Settings.MobileHelpAndFeedback");
            }
            return true;
        }

        if (item.getItemId() == android.R.id.home) {
            handleHomeAsUp(activity, delegate);
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            RecordUserAction.record("Settings.MobileHelpAndFeedback");
            delegate.getHelpAndFeedbackLauncher()
                    .show(activity, activity.getString(R.string.help_context_settings), null);
            return true;
        }
        return false;
    }

    /**
     * Helper to handle the "home as up" (back) navigation.
     *
     * @param activity The Activity hosting the settings.
     * @param delegate The Delegate to handle specific actions.
     */
    public static void handleHomeAsUp(Activity activity, Delegate delegate) {
        Fragment mainFragment = delegate.getMainFragment();
        MultiColumnSettings multiColumnSettings = delegate.getMultiColumnSettings();
        SettingsSearchCoordinator searchCoordinator = delegate.getSearchCoordinator();
        if (multiColumnSettings != null) {
            if (multiColumnSettings.isTwoColumn()) {
                // In two pane mode, selecting back always exits from the settings activity.
                // TODO(crbug.com/521895796): Update this for settings-in-a-tab.
                delegate.finishSettings();
            } else {
                // PreferenceHeaderFragmentCompat implements back button behavior.
                // In order to forward the event to there, translate the event to the back
                // button.
                delegate.onBackPressed();
            }
        } else if (!(searchCoordinator != null && searchCoordinator.handleBackAction())) {
            // Search UI may handle the back action if it's showing its own fragment. Finish
            // the main fragment only it didn't.
            delegate.finishCurrentSettings(assumeNonNull(mainFragment));
        }
    }

    /**
     * Configures the navigation icon and click listener on the toolbar based on column layout. The
     * Chrome logo is shown in multi-column layouts, or in single-column layouts when SettingsInTab
     * is enabled and showing the top-level main settings. A back button is shown in single-column
     * layouts otherwise.
     */
    public static void updateNavigationIcon(
            Toolbar toolbar,
            Activity activity,
            boolean show,
            boolean isMultiColumn,
            boolean isMainSettings) {
        if (show) {
            if (isMultiColumn || (SettingsInTab.isEnabled() && isMainSettings)) {
                // Show the Chrome logo at 32x32 dp without tinting.
                toolbar.setNavigationIcon(R.drawable.app_icon_32dp);
                if (toolbar instanceof MaterialToolbar materialToolbar) {
                    materialToolbar.clearNavigationIconTint();
                }
                toolbar.setNavigationOnClickListener(null);
                toolbar.setNavigationContentDescription(activity.getString(R.string.app_name));
            } else {
                // Show a back button.
                toolbar.setNavigationIcon(R.drawable.ic_arrow_back_24dp);
                toolbar.setNavigationOnClickListener(v -> activity.onBackPressed());
                toolbar.setNavigationContentDescription(activity.getString(R.string.back));
            }
        } else {
            // Hide the icon.
            toolbar.setNavigationIcon(null);
        }
    }
}
