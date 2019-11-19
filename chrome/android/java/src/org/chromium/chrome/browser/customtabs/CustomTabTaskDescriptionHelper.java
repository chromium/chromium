// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.NavigationHandle;

import javax.inject.Inject;

/**
 * Helper that updates the Android task description given the state of the current tab.
 *
 * <p>
 * The task description is what is shown in Android's Overview/Recents screen for each entry.
 */
@ActivityScope
public class CustomTabTaskDescriptionHelper implements NativeInitObserver, Destroyable {
    private final ChromeActivity mActivity;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;

    @Nullable
    private CustomTabTaskDescriptionIconGenerator mIconGenerator;
    @Nullable
    private FaviconHelper mFaviconHelper;

    private TabObserver mTabObserver;
    private CustomTabActivityTabProvider.Observer mActivityTabObserver;

    private int mDefaultThemeColor;

    private Bitmap mLargestFavicon;

    /**
     * Constructs a task description helper for the given activity.
     *
     * @param activity The activity whose descriptions should be updated.
     * @param defaultThemeColor The default theme color to be used if the tab does not override it.
     */
    @Inject
    public CustomTabTaskDescriptionHelper(ChromeActivity activity,
            CustomTabActivityTabProvider tabProvider, TabObserverRegistrar tabObserverRegistrar,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;

        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP || !usesSeparateTask()) return;

        startObserving();
    }

    public void startObserving() {
        mDefaultThemeColor = ApiCompatibilityUtils.getColor(
                mActivity.getResources(), R.color.default_primary_color);

        mIconGenerator = new CustomTabTaskDescriptionIconGenerator(mActivity);
        mFaviconHelper = new FaviconHelper();

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                if (!didStartLoad) return;
                resetIcon();
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                if (icon == null) return;
                updateFavicon(icon);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onSSLStateUpdated(Tab tab) {
                if (hasSecurityWarningOrError(tab)) resetIcon();
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()
                        && !navigation.isSameDocument()) {
                    mLargestFavicon = null;
                    updateTaskDescription();
                }
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                updateTaskDescription();
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateTaskDescription();
            }

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                resetIcon();
            }

            @Override
            public void onDidDetachInterstitialPage(Tab tab) {
                resetIcon();
            }

            private boolean hasSecurityWarningOrError(Tab tab) {
                int securityLevel = tab.getSecurityLevel();
                return securityLevel == ConnectionSecurityLevel.DANGEROUS;
            }
        };

        mActivityTabObserver = new CustomTabActivityTabProvider.Observer() {
            @Override
            public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
                fetchIcon();
                updateTaskDescription();
            }

            @Override
            public void onTabSwapped(@NonNull Tab tab) {
                fetchIcon();
                updateTaskDescription();
            }
        };

        mTabObserverRegistrar.registerActivityTabObserver(mTabObserver);
        mTabProvider.addObserver(mActivityTabObserver);

        fetchIcon();
        updateTaskDescription();
    }

    private void resetIcon() {
        mLargestFavicon = null;
        updateTaskDescription();
    }

    private void updateFavicon(Bitmap favicon) {
        if (favicon == null) return;
        if (mLargestFavicon == null || favicon.getWidth() > mLargestFavicon.getWidth()
                || favicon.getHeight() > mLargestFavicon.getHeight()) {
            mLargestFavicon = favicon;
            updateTaskDescription();
        }
    }

    private void updateTaskDescription() {
        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) {
            updateTaskDescription(null, null);
            return;
        }

        if (NewTabPage.isNTPUrl(currentTab.getUrl()) && !currentTab.isIncognito()) {
            // NTP needs a new color in recents, but uses the default application title and icon
            updateTaskDescription(null, null);
            return;
        }

        String label = currentTab.getTitle();
        String domain = UrlUtilities.getDomainAndRegistry(currentTab.getUrl(), false);
        if (TextUtils.isEmpty(label)) {
            label = domain;
        }
        if (mLargestFavicon == null && TextUtils.isEmpty(label)) {
            updateTaskDescription(null, null);
            return;
        }

        Bitmap bitmap = null;
        if (!currentTab.isIncognito()) {
            bitmap = mIconGenerator.getBitmap(currentTab.getUrl(), mLargestFavicon);
        }

        updateTaskDescription(label, bitmap);
    }

    /**
     * Update the task description with the specified icon and label.
     *
     * <p>
     * This is only publicly visible to allow activities to set this early during initialization
     * prior to the tab's being available.
     *
     * @param label The text to use in the task description.
     * @param icon The icon to use in the task description.
     */
    public void updateTaskDescription(String label, Bitmap icon) {
        Tab currentTab = mTabProvider.getTab();
        int color = mDefaultThemeColor;
        if (currentTab != null) {
            if (!TabThemeColorHelper.isDefaultColorUsed(currentTab)) {
                color = TabThemeColorHelper.getColor(currentTab);
            }
        }
        ApiCompatibilityUtils.setTaskDescription(mActivity, label, icon, color);
    }

    private void fetchIcon() {
        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return;

        final String currentUrl = currentTab.getUrl();
        mFaviconHelper.getLocalFaviconImageForURL(
                currentTab.getProfile(), currentTab.getUrl(), 0, (image, iconUrl) -> {
                    if (mTabProvider.getTab() == null
                            || !TextUtils.equals(currentUrl, mTabProvider.getTab().getUrl())) {
                        return;
                    }

                    updateFavicon(image);
                });
    }

    /**
     * Returns true when the activity has been launched in a separate task.
     */
    private boolean usesSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (mActivity.getIntent().getFlags() & separateTaskFlags) != 0;
    }

    /**
     * Destroys all dependent components of the task description helper.
     */
    @Override
    public void destroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
        }

        mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
        mTabProvider.removeObserver(mActivityTabObserver);
    }
}
