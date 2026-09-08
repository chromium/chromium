// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/** Delegates settings preference navigation calls to {@link Tab#loadUrl} for in-tab navigation. */
@NullMarked
public class SettingsInTabNavigationDelegate implements SettingsNavigation {
    private final Tab mTab;

    /**
     * Constructs a navigation delegate bound to a specific tab.
     *
     * @param tab Target {@link Tab} where settings URL loads will be executed.
     */
    public SettingsInTabNavigationDelegate(Tab tab) {
        mTab = tab;
    }

    @Override
    public void startSettings(Context context) {
        startSettings(context, SettingsFragment.MAIN);
    }

    @Override
    public void startSettings(Context context, @SettingsFragment int settingsFragment) {
        startSettings(context, settingsFragment, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context, @SettingsFragment int settingsFragment, boolean addToBackStack) {
        Class<? extends Fragment> fragmentClass =
                SettingsNavigationImpl.getFragmentClassFromEnum(settingsFragment);
        startSettings(context, fragmentClass, null, addToBackStack);
    }

    @Override
    public void startSettings(Context context, @Nullable Class<? extends Fragment> fragment) {
        startSettings(context, fragment, null);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        startSettings(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        startSettings(context, fragment, fragmentArgs, addToBackStack, /* tag= */ null);
    }

    @Override
    public void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag) {
        // A null fragment here means load the default page (chrome://settings).
        if (fragment == null) {
            mTab.loadUrl(new LoadUrlParams(UrlConstants.SETTINGS_URL));
            return;
        }

        String targetUrl = SettingsFragmentRegistry.createUrlForFragment(fragment, fragmentArgs);
        if (targetUrl != null) {
            // Executing loadUrl updates the Omnibox, creates a WebContents navigation history
            // entry, and triggers SettingsPage.updateForUrl() on the current tab.
            mTab.loadUrl(new LoadUrlParams(targetUrl));
        } else {
            // Unmapped fragments fall back to launching an Intent so un-migrated subpages continue
            // to function.
            Intent intent =
                    createSettingsIntent(context, fragment, fragmentArgs, addToBackStack, tag);
            context.startActivity(intent);
        }
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        return createSettingsIntent(context, fragment, null);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs) {
        return createSettingsIntent(context, fragment, fragmentArgs, /* addToBackStack= */ false);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack) {
        return createSettingsIntent(
                context, fragment, fragmentArgs, addToBackStack, /* tag= */ null);
    }

    @Override
    public Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs,
            boolean addToBackStack,
            @Nullable String tag) {
        String fragmentName = fragment == null ? null : fragment.getName();
        // Use SettingsIntentUtil directly with useSettingsInTab = true.
        // This generates an Intent targeting ChromeLauncherActivity for in-tab
        // navigation while retaining fallback behavior for standalone fragments.
        return SettingsIntentUtil.createIntent(
                context,
                fragmentName,
                fragmentArgs,
                addToBackStack,
                tag,
                /* useSettingsInTab= */ true);
    }

    @Override
    public Intent createSettingsIntent(
            Context context, @SettingsFragment int fragment, @Nullable Bundle fragmentArgs) {
        Class<? extends Fragment> fragmentClass =
                SettingsNavigationImpl.getFragmentClassFromEnum(fragment);
        return createSettingsIntent(context, fragmentClass, fragmentArgs);
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        // Prefer looking up the enclosing SettingsHostFragment directly from the fragment's parent
        // hierarchy. When Chrome is in the background or during lifecycle transitions, the host
        // fragment view may not report isShown(), which causes the activity-level lookup to return
        // null.
        SettingsHostFragment hostFragment = SettingsHostFragment.get(fragment);
        if (hostFragment == null) {
            WindowAndroid windowAndroid = mTab.getWindowAndroid();
            Activity activity = windowAndroid != null ? windowAndroid.getActivity().get() : null;
            hostFragment = SettingsHostFragment.get(activity);
        }
        if (hostFragment != null) {
            hostFragment.finishCurrentSettings(fragment);
        }
    }

    @Override
    public void executePendingNavigations(Activity activity) {
        SettingsHostFragment hostFragment = SettingsHostFragment.get(activity);
        if (hostFragment != null) {
            hostFragment.executePendingNavigations();
        }
    }

    @Override
    public void setUseSettingsActivityForTesting(boolean useSettingsActivity) {
        // No-op for in-tab navigation delegate.
    }
}
