// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.embedder_support.util.UrlConstants;

@NullMarked
public class SettingsIntentUtil {
    private static final String TAG = "SettingsIntentUtil";

    // The last intent used to launch settings. Temporary workaround for navigating to settings
    // sub-pages like Downloads under SettingsInTab. This will be replaced once SettingsInTabUrlNav
    // launches. See
    // https://crbug.com/559534170.
    private static @Nullable Intent sLastIntent;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";

    public static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    public static final String EXTRA_SHOW_FRAGMENT_STANDALONE = "show_fragment_standalone";
    public static final String EXTRA_ADD_TO_BACK_STACK = "add_to_back_stack";
    public static final String EXTRA_FRAGMENT_TAG = "fragment_tag";

    private SettingsIntentUtil() {}

    /**
     * Returns the last intent used to launch settings, or null if there isn't one. The saved intent
     * is cleared, so it is not reused by settings tabs opened later in the session.
     */
    public static @Nullable Intent takeLastIntent() {
        Intent intent = sLastIntent;
        sLastIntent = null;
        return intent;
    }

    /** Sets the last intent used to launch settings for testing. */
    public static void setLastIntentForTesting(@Nullable Intent intent) {
        sLastIntent = intent;
        ResettersForTesting.register(() -> sLastIntent = null);
    }

    /**
     * Creates an {@link Intent} that launches the settings activity.
     *
     * @param context The context from which the settings activity is being launched.
     * @param fragmentName The name of the main fragment shown in the settings activity. null means
     *     the default fragment.
     * @param fragmentArgs A bundle of extra arguments given to the main fragment. Can be null.
     * @return An intent ready to launch the settings activity.
     */
    public static Intent createIntent(
            Context context, @Nullable String fragmentName, @Nullable Bundle fragmentArgs) {
        return createIntent(context, fragmentName, fragmentArgs, /* addToBackStack= */ false);
    }

    /**
     * Creates an {@link Intent} that launches the settings activity.
     *
     * @param context The context from which the settings activity is being launched.
     * @param fragmentName The name of the main fragment shown in the settings activity. null means
     *     the default fragment.
     * @param fragmentArgs A bundle of extra arguments given to the main fragment. Can be null.
     * @param addToBackStack if true, the fragment will be added to fragment manager's back stack.
     * @return An intent ready to launch the settings activity.
     */
    public static Intent createIntent(
            Context context,
            @Nullable String fragmentName,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        return createIntent(
                context,
                fragmentName,
                fragmentArgs,
                addToBackStack,
                /* tag= */ null,
                SettingsInTab.shouldOpenSettingsInTab());
    }

    /**
     * Creates an {@link Intent} that launches the settings activity.
     *
     * @param context The context from which the settings activity is being launched.
     * @param fragmentName The name of the main fragment shown in the settings activity. null means
     *     the default fragment.
     * @param fragmentArgs A bundle of extra arguments given to the main fragment. Can be null.
     * @param addToBackStack if true, the fragment will be added to fragment manager's back stack.
     * @param tag A tag used to identify the fragment transaction.
     * @param useSettingsInTab whether to use SettingsInTab (if available). Pass false to force the
     *     use of SettingsActivity.
     * @return An intent ready to launch the settings activity.
     */
    public static Intent createIntent(
            Context context,
            @Nullable String fragmentName,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag,
            boolean useSettingsInTab) {
        Intent intent = new Intent();
        Class<?> fragmentClass = loadFragmentClass(context, fragmentName);
        boolean isStandaloneFragment =
                fragmentClass != null
                        && !EmbeddableSettingsPage.class.isAssignableFrom(fragmentClass);
        String targetUrl = null;
        if (useSettingsInTab && !isStandaloneFragment) {
            intent.setAction(Intent.ACTION_VIEW);
            if (ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()
                    && fragmentClass != null
                    && Fragment.class.isAssignableFrom(fragmentClass)) {
                targetUrl =
                        SettingsFragmentRegistry.createUrlForFragment(
                                fragmentClass.asSubclass(Fragment.class), fragmentArgs);
                // A URL that cannot carry this page's arguments is not a substitute for the
                // intent. Under URL navigation the URL is what the page is rebuilt from on tab
                // restore, on back, and on any later replay from history, so an argument it drops
                // - or, for an argument with no registered typed parser, silently turns into a
                // String - is lost for good. Fall back to the intent, which carries the Bundle
                // verbatim, and leave the page reachable until the argument is registered in
                // SettingsFragmentRegistry.
                if (targetUrl != null
                        && !SettingsFragmentRegistry.urlPreservesArgs(targetUrl, fragmentArgs)) {
                    targetUrl = null;
                }
            }
            intent.setData(Uri.parse(targetUrl != null ? targetUrl : UrlConstants.SETTINGS_URL));
            intent.setClass(context, ChromeLauncherActivity.class);
            // Internal chrome URLs require trusted intents.
            IntentUtils.addTrustedIntentExtras(intent);
        } else {
            intent.setClass(context, SettingsActivity.class);
            if (isStandaloneFragment) {
                intent.putExtra(EXTRA_SHOW_FRAGMENT_STANDALONE, true);
            } else if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
                // Note that this intent will be delivered to an existing settings activity (if it
                // exists) even if it is hosting a standalone fragment. In this case, the activity
                // will resend the intent without the flag to start a new activity. See
                // SettingsActivity#onNewIntent.
                intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            }
        }
        if (!(context instanceof Activity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        if (fragmentName != null) {
            intent.putExtra(EXTRA_SHOW_FRAGMENT, fragmentName);
        }
        if (fragmentArgs != null) {
            intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
        if (addToBackStack) {
            intent.putExtra(EXTRA_ADD_TO_BACK_STACK, addToBackStack);
            if (tag != null) intent.putExtra(EXTRA_FRAGMENT_TAG, tag);
        }
        if (useSettingsInTab && !isStandaloneFragment) {
            // Clear rather than merely skip when the URL says everything the intent would have.
            // sLastIntent is written when an intent is created, not when it is started, so one
            // built for a notification that is never tapped stays behind indefinitely. The next
            // settings tab consumes it in SettingsHostFragment#onViewCreated, where a pending
            // intent outranks the initial URL, and would open the wrong page.
            sLastIntent = targetUrl == null ? intent : null;
        }
        return intent;
    }

    /**
     * Loads the fragment class named by {@code fragmentName}, or null if it names nothing.
     *
     * <p>Used to decide whether a fragment is standalone, i.e. whether it does not implement {@link
     * EmbeddableSettingsPage}. Such fragments are shown in separate activities and have full
     * control over the whole UI. See {@link SettingsActivity} for details.
     */
    private static @Nullable Class<?> loadFragmentClass(
            Context context, @Nullable String fragmentName) {
        if (fragmentName == null) {
            return null;
        }

        try {
            return context.getClassLoader().loadClass(fragmentName);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }
}
