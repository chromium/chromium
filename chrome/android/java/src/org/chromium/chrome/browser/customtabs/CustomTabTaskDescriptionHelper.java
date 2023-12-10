// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import javax.inject.Inject;

/**
 * Helper that updates the Android task description given the state of the current tab.
 *
 * <p>
 * The task description is what is shown in Android's Overview/Recents screen for each entry.
 */
@ActivityScope
public class CustomTabTaskDescriptionHelper implements NativeInitObserver, DestroyObserver {
    private final Activity mActivity;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Nullable private CustomTabTaskDescriptionIconGenerator mIconGenerator;
    @Nullable private FaviconHelper mFaviconHelper;

    @Nullable private CustomTabTabObserver mTabObserver;
    @Nullable private CustomTabTabObserver mIconTabObserver;
    @Nullable private CustomTabActivityTabProvider.Observer mActivityTabObserver;

    private int mDefaultThemeColor;
    @Nullable private String mForceTitle;
    @Nullable private Bitmap mForceIcon;
    private boolean mUseClientIcon;

    @Nullable private Bitmap mLargestFavicon;

    @Inject
    public CustomTabTaskDescriptionHelper(
            Activity activity,
            CustomTabActivityTabProvider tabProvider,
            TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
        mIntentDataProvider = intentDataProvider;
        mTopUiThemeColorProvider = topUiThemeColorProvider;

        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        boolean canUpdate = (webappExtras != null || usesSeparateTask());
        if (!canUpdate) return;

        mDefaultThemeColor = mActivity.getColor(R.color.default_primary_color);
        if (webappExtras != null) {
            if (mIntentDataProvider.getColorProvider().hasCustomToolbarColor()) {
                mDefaultThemeColor = mIntentDataProvider.getColorProvider().getToolbarColor();
            }
            mForceIcon = webappExtras.icon.bitmap();
            mForceTitle = webappExtras.shortName;

            // This is a workaround for crbug/1098580. ActivityManager.TaskDescription
            // does not handle adaptive icon when passing a bitmap. So set the task icon to be null
            // to preserve the client app's icon. Only set this flag on O+ because this does not
            // work with old_style_webapk.
            if (mIntentDataProvider.isWebApkActivity()
                    && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                mUseClientIcon = true;
            }
        }

        mIconGenerator = new CustomTabTaskDescriptionIconGenerator(mActivity);
        mFaviconHelper = new FaviconHelper();

        startObserving();
    }

    private void startObserving() {
        mTabObserver =
                new CustomTabTabObserver() {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateTaskDescription();
                    }

                    @Override
                    public void onTitleUpdated(Tab tab) {
                        updateTaskDescription();
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (navigation.hasCommitted() && !navigation.isSameDocument()) {
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
                    public void onAttachedToInitialTab(@NonNull Tab tab) {
                        onActiveTabChanged();
                    }

                    @Override
                    public void onObservingDifferentTab(@NonNull Tab tab) {
                        onActiveTabChanged();
                    }
                };
        mTabObserverRegistrar.registerActivityTabObserver(mTabObserver);

        if (mForceIcon == null && !mUseClientIcon) {
            mIconTabObserver =
                    new CustomTabTabObserver() {
                        @Override
                        public void onWebContentsSwapped(
                                Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                            if (!didStartLoad) return;
                            resetIcon();
                        }

                        @Override
                        public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
                            if (icon == null) return;
                            updateFavicon(icon);
                        }

                        @Override
                        public void onSSLStateUpdated(Tab tab) {
                            if (hasSecurityWarningOrError(tab)) resetIcon();
                        }

                        private boolean hasSecurityWarningOrError(Tab tab) {
                            boolean isContentDangerous =
                                    SecurityStateModel.isContentDangerous(tab.getWebContents());
                            return isContentDangerous;
                        }
                    };
            mTabObserverRegistrar.registerActivityTabObserver(mIconTabObserver);
        }
    }

    private void stopObserving() {
        mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
        mTabObserverRegistrar.unregisterActivityTabObserver(mIconTabObserver);
    }

    private void onActiveTabChanged() {
        updateTaskDescription();
        if (mForceIcon == null) {
            fetchIcon();
        }
    }

    private void resetIcon() {
        mLargestFavicon = null;
        updateTaskDescription();
    }

    private void updateFavicon(Bitmap favicon) {
        if (favicon == null) return;
        if (mLargestFavicon == null
                || favicon.getWidth() > mLargestFavicon.getWidth()
                || favicon.getHeight() > mLargestFavicon.getHeight()) {
            mLargestFavicon = favicon;
            updateTaskDescription();
        }
    }

    private void updateTaskDescription() {
        mActivity.setTaskDescription(
                new ActivityManager.TaskDescription(
                        computeTitle(), computeIcon(), computeThemeColor()));
    }

    /** Computes the title for the task description. */
    private String computeTitle() {
        if (!TextUtils.isEmpty(mForceTitle)) return mForceTitle;

        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return null;

        String label = currentTab.getTitle();
        String domain = UrlUtilities.getDomainAndRegistry(currentTab.getUrl().getSpec(), false);
        if (TextUtils.isEmpty(label)) {
            label = domain;
        }
        return label;
    }

    /** Computes the icon for the task description. */
    private Bitmap computeIcon() {
        if (mUseClientIcon) return null;

        if (mForceIcon != null) return mForceIcon;

        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return null;

        Bitmap bitmap = null;
        if (!currentTab.isIncognito()) {
            bitmap = mIconGenerator.getBitmap(currentTab.getUrl(), mLargestFavicon);
        }
        return bitmap;
    }

    /** Computes the theme color for the task description. */
    private int computeThemeColor() {
        Tab tab = mTabProvider.getTab();
        int themeColor = mTopUiThemeColorProvider.getThemeColorOrFallback(tab, mDefaultThemeColor);
        return ColorUtils.getOpaqueColor(themeColor);
    }

    private void fetchIcon() {
        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return;

        final GURL currentUrl = currentTab.getUrl();
        mFaviconHelper.getLocalFaviconImageForURL(
                currentTab.getProfile(),
                currentTab.getUrl(),
                0,
                (image, iconUrl) -> {
                    if (mTabProvider.getTab() == null
                            || !currentUrl.equals(mTabProvider.getTab().getUrl())) {
                        return;
                    }

                    updateFavicon(image);
                });
    }

    /** Returns true when the activity has been launched in a separate task. */
    private boolean usesSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (mActivity.getIntent().getFlags() & separateTaskFlags) != 0;
    }

    /** Destroys all dependent components of the task description helper. */
    @Override
    public void onDestroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
        }
        stopObserving();
    }
}
