// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
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
            return;
        }

        // Some pages have no URL, either because they have not been migrated yet or because they
        // cannot have one: ChosenObjectSettings, for instance, is identified by a serialized device
        // descriptor. Show them in the current host on the fragment back stack. Launching an Intent
        // here would open a second settings tab at the root URL, which is never what the user
        // asked for from inside settings.
        SettingsHostFragment hostFragment = findHostFragment(/* fragment= */ null);
        if (hostFragment != null) {
            hostFragment.showFragment(
                    Fragment.instantiate(context, fragment.getName(), fragmentArgs),
                    // Always added to the back stack, whatever the caller asked for. This page has
                    // no navigation entry of its own, so the fragment back stack is the only thing
                    // a back press has to pop; passing the caller's addToBackStack through would
                    // strand the user on a page with no way back.
                    /* addToBackStack= */ true,
                    tag);
            return;
        }

        Intent intent = createSettingsIntent(context, fragment, fragmentArgs, addToBackStack, tag);
        context.startActivity(intent);
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
        finishCurrentSettings(fragment, /* parentFragment= */ null, /* parentArgs= */ null);
    }

    @Override
    public void finishCurrentSettings(
            Fragment fragment,
            @Nullable Class<? extends Fragment> parentFragment,
            @Nullable Bundle parentArgs) {
        SettingsHostFragment hostFragment = findHostFragment(fragment);

        // Anything the host can handle itself, let it: it ignores a call naming a page that is no
        // longer current or already finished, it defers one made while fragment state is saved,
        // and it pops the back stack for a page shown by a fragment transaction, e.g. one opened
        // from a page that has not been migrated to URL navigation. Only a page shown by URL
        // navigation, which has no back stack entry to pop, is worth intercepting.
        if (hostFragment != null && !hostFragment.wouldShowMainSettings(fragment)) {
            hostFragment.finishCurrentSettings(fragment);
            return;
        }

        // The page occupies a navigation entry in the tab, so land somewhere deliberate.
        String parentUrl =
                parentFragment == null
                        ? null
                        : SettingsFragmentRegistry.createUrlForFragment(parentFragment, parentArgs);
        if (parentUrl != null) {
            navigateReplacingCurrentEntry(parentUrl);
            return;
        }
        if (goBackToPreviousSettingsEntry()) {
            return;
        }

        // No settings entry to return to, e.g. settings was the first page loaded in this tab.
        // Let the host show the main settings page after all.
        if (hostFragment != null) {
            hostFragment.finishCurrentSettings(fragment);
        }
    }

    /**
     * Replaces the entry currently being navigated to with {@code url}.
     *
     * <p>For a URL that turned out not to name a page that can be shown, discovered while the tab
     * is resolving a navigation to it. The replacement is deferred to the next message rather than
     * run inline, because the navigation that led here is still being committed and starting
     * another one underneath it is not something the navigation controller expects.
     */
    void redirectFromNavigation(String url) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (!mTab.isInitialized()) return;
                    navigateReplacingCurrentEntry(url);
                });
    }

    /**
     * Navigates to {@code url}, replacing the current navigation entry instead of pushing a new
     * one.
     *
     * <p>For leaving a page whose entry is no longer valid, e.g. because the data it was showing
     * was deleted. Replacing rather than pushing keeps the dead entry out of history, so going back
     * does not return to it.
     *
     * <p>If the previous entry already shows {@code url}, goes back to it instead, so the user is
     * not left with two adjacent entries for the same page and a back press that appears to do
     * nothing.
     */
    void navigateReplacingCurrentEntry(String url) {
        if (SettingsFragmentRegistry.isSameSettingsPage(getPreviousEntryUrl(), url)) {
            mTab.goBack();
            return;
        }
        LoadUrlParams params = new LoadUrlParams(url);
        params.setShouldReplaceCurrentEntry(true);
        mTab.loadUrl(params);
    }

    /**
     * Goes back if the previous navigation entry is a settings page, and returns whether it did.
     *
     * <p>The previous entry is deliberately required to be a settings page: going back out of
     * settings altogether is not what finishing a settings page means.
     */
    private boolean goBackToPreviousSettingsEntry() {
        String previousUrl = getPreviousEntryUrl();
        if (previousUrl == null
                || SettingsFragmentRegistry.getFragmentClassForUrl(previousUrl) == null) {
            return false;
        }
        mTab.goBack();
        return true;
    }

    /** Returns the URL of the entry preceding the current one, or null if there is none. */
    private @Nullable String getPreviousEntryUrl() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return null;

        NavigationController controller = webContents.getNavigationController();
        if (!controller.canGoBack()) return null;

        NavigationHistory history =
                controller.getDirectedNavigationHistory(/* isForward= */ false, /* itemLimit= */ 1);
        if (history == null || history.getEntryCount() == 0) return null;
        return history.getEntryAtIndex(0).getUrl().getSpec();
    }

    /**
     * Resolves the host fragment, preferring the given fragment's own parent hierarchy. When Chrome
     * is in the background or during lifecycle transitions the host fragment's view may not report
     * isShown(), which makes the activity-level lookup return null.
     */
    private @Nullable SettingsHostFragment findHostFragment(@Nullable Fragment fragment) {
        if (fragment != null) {
            SettingsHostFragment hostFragment = SettingsHostFragment.get(fragment);
            if (hostFragment != null) return hostFragment;
        }

        WindowAndroid windowAndroid = mTab.getWindowAndroid();
        Activity activity = windowAndroid != null ? windowAndroid.getActivity().get() : null;
        return SettingsHostFragment.get(activity);
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
