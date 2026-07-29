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

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.embedder_support.util.UrlConstants;

@NullMarked
public class SettingsIntentUtil {
    private static final String TAG = "SettingsIntentUtil";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";

    public static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    public static final String EXTRA_SHOW_FRAGMENT_STANDALONE = "show_fragment_standalone";
    public static final String EXTRA_ADD_TO_BACK_STACK = "add_to_back_stack";
    public static final String EXTRA_FRAGMENT_TAG = "fragment_tag";

    private SettingsIntentUtil() {}

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
                SettingsInTab.isEnabled());
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
        boolean isStandaloneFragment = isStandaloneFragment(context, fragmentName);
        if (useSettingsInTab && !isStandaloneFragment) {
            intent.setAction(Intent.ACTION_VIEW);
            // TODO(crbug.com/521895796): When URLs for settings subpages exist (e.g.
            // chrome://settings/appearance) use them and stop adding fragment information
            // below.
            intent.setData(Uri.parse(UrlConstants.SETTINGS_URL));
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
        return intent;
    }

    /**
     * Checks if a given fragment is a standalone fragment.
     *
     * <p>A fragment is standalone if it does not implement {@link EmbeddableSettingsPage}. Such
     * fragments are shown in separate activities and have full control over the whole UI. See
     * {@link SettingsActivity} for details.
     */
    private static boolean isStandaloneFragment(Context context, @Nullable String fragmentName) {
        if (fragmentName == null) {
            return false;
        }

        Class<?> fragmentClass;
        try {
            fragmentClass = context.getClassLoader().loadClass(fragmentName);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }

        return !EmbeddableSettingsPage.class.isAssignableFrom(fragmentClass);
    }
}
