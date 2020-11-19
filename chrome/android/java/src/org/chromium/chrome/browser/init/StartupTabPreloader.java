// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * This class attempts to preload the tab if the url is known from the intent when the profile
 * is created. This is done to improve startup latency.
 */
public class StartupTabPreloader implements ProfileManager.Observer, Destroyable {
    private static final String EXTRA_DISABLE_STARTUP_TAB_PRELOADER =
            "org.chromium.chrome.browser.init.DISABLE_STARTUP_TAB_PRELOADER";

    private final Supplier<Intent> mIntentSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final TabCreatorManager mTabCreatorManager;
    private final IntentHandler mIntentHandler;
    private LoadUrlParams mLoadUrlParams;
    private Tab mTab;
    private StartupTabObserver mObserver;
    private Callback<Tab> mTabCreatedCallback;

    public StartupTabPreloader(Supplier<Intent> intentSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher, WindowAndroid windowAndroid,
            TabCreatorManager tabCreatorManager, IntentHandler intentHandler) {
        mIntentSupplier = intentSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        mTabCreatorManager = tabCreatorManager;
        mIntentHandler = intentHandler;

        mActivityLifecycleDispatcher.register(this);
        ProfileManager.addObserver(this);
    }

    @Override
    public void destroy() {
        if (mTab != null) mTab.destroy();
        mTab = null;

        ProfileManager.removeObserver(this);
        mActivityLifecycleDispatcher.unregister(this);
    }

    public void setTabCreatedCallback(Callback<Tab> callback) {
        mTabCreatedCallback = callback;
    }

    /**
     * Returns the Tab if loadUrlParams and type match, otherwise the Tab is discarded.
     *
     * @param loadUrlParams The actual parameters of the url load.
     * @param type The actual launch type type.
     * @return The results of maybeNavigate() if they match loadUrlParams and type or null
     *         otherwise.
     */
    public Tab takeTabIfMatchingOrDestroy(LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        if (mTab == null) return null;

        boolean tabMatches = type == mTab.getLaunchType()
                && doLoadUrlParamsMatchForWarmupManagerNavigation(mLoadUrlParams, loadUrlParams);

        RecordHistogram.recordBooleanHistogram(
                "Startup.Android.StartupTabPreloader.TabTaken", tabMatches);

        if (!tabMatches) {
            mTab.destroy();
            mTab = null;
            mLoadUrlParams = null;
            return null;
        }

        Tab tab = mTab;
        mTab = null;
        mLoadUrlParams = null;
        mTabCreatedCallback = null;
        tab.removeObserver(mObserver);
        return tab;
    }

    @VisibleForTesting
    static boolean doLoadUrlParamsMatchForWarmupManagerNavigation(
            LoadUrlParams preconnectParams, LoadUrlParams loadParams) {
        if (!TextUtils.equals(preconnectParams.getUrl(), loadParams.getUrl())) return false;

        String preconnectReferrer = preconnectParams.getReferrer() != null
                ? preconnectParams.getReferrer().getUrl()
                : null;
        String loadParamsReferrer =
                loadParams.getReferrer() != null ? loadParams.getReferrer().getUrl() : null;

        return TextUtils.equals(preconnectReferrer, loadParamsReferrer);
    }

    /**
     * Called by the ProfileManager when a profile has been created. This occurs during startup
     * and it's the earliest point at which we can create and load a tab. If the url can be
     * determined from the intent, then a tab will be loaded and potentially adopted by
     * {@link ChromeTabCreator}.
     */
    @Override
    public void onProfileAdded(Profile profile) {
        try (TraceEvent e = TraceEvent.scoped("StartupTabPreloader.onProfileAdded")) {
            // We only care about the first non-incognito profile that's created during startup.
            if (profile.isOffTheRecord()) return;

            ProfileManager.removeObserver(this);
            boolean shouldLoad = shouldLoadTab();
            if (shouldLoad) loadTab();
            RecordHistogram.recordBooleanHistogram(
                    "Startup.Android.StartupTabPreloader.TabLoaded", shouldLoad);
        }
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    /**
     * @returns True if based on the intent we should load the tab, returns false otherwise.
     */
    @VisibleForTesting
    boolean shouldLoadTab() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)) {
            return false;
        }

        // If mTab isn't null we've been called before and there is nothing to do.
        if (mTab != null) return false;

        Intent intent = mIntentSupplier.get();
        if (IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_STARTUP_TAB_PRELOADER, false)) {
            return false;
        }
        if (mIntentHandler.shouldIgnoreIntent(intent, /*startedActivity=*/true)) return false;
        if (getUrlFromIntent(intent) == null) return false;

        // We don't support incognito tabs because only chrome can send new incognito tab
        // intents and that's not a startup scenario.
        boolean incognito = IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
        if (incognito) return false;

        // The TabCreatorManager throws an IllegalStateException if it is not ready to provide a
        // TabCreator.
        TabCreator tabCreator;
        try {
            tabCreator = mTabCreatorManager.getTabCreator(incognito);
        } catch (IllegalStateException e) {
            return false;
        }

        // We want to get the TabDelegateFactory but only ChromeTabCreator has one.
        if (!(tabCreator instanceof ChromeTabCreator)) return false;

        return true;
    }

    private void loadTab() {
        Intent intent = mIntentSupplier.get();
        GURL url = UrlFormatter.fixupUrl(getUrlFromIntent(intent));

        ChromeTabCreator chromeTabCreator =
                (ChromeTabCreator) mTabCreatorManager.getTabCreator(false);
        WebContents webContents =
                WebContentsFactory.createWebContents(Profile.getLastUsedRegularProfile(), false);

        mLoadUrlParams = new LoadUrlParams(url.getValidSpecOrEmpty());
        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);
        if (referrer != null && !referrer.isEmpty()) {
            mLoadUrlParams.setReferrer(new Referrer(referrer, ReferrerPolicy.DEFAULT));
        }
        int transition = IntentHandler.getTransitionTypeFromIntent(
                intent, PageTransition.LINK | PageTransition.FROM_API);
        mLoadUrlParams.setTransitionType(transition);

        // Create a detached tab, but don't add it to the tab model yet. We'll do that
        // later if the loadUrlParams etc... match.
        mTab = TabBuilder.createLiveTab(false)
                       .setIncognito(false)
                       .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                       .setWindow(mWindowAndroid)
                       .setWebContents(webContents)
                       .setDelegateFactory(chromeTabCreator.createDefaultTabDelegateFactory())
                       .build();
        if (mTabCreatedCallback != null) mTabCreatedCallback.onResult(mTab);

        mObserver = new StartupTabObserver();
        mTab.addObserver(mObserver);
        mTab.loadUrl(mLoadUrlParams);
    }

    private static String getUrlFromIntent(Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_VIEW.equals(action) || Intent.ACTION_MAIN.equals(action)
                || (action == null
                        && ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME.equals(
                                intent.getComponent().getClassName()))) {
            // TODO(alexclarke): For ACTION_MAIN maybe refactor TabPersistentStore so we can
            // instantiate (a subset of that) here to extract the URL if it's not set in the
            // intent.
            return IntentHandler.getUrlFromIntent(intent);
        } else {
            return null;
        }
    }

    private class StartupTabObserver extends EmptyTabObserver {
        @Override
        public void onCrash(Tab tab) {
            destroy();
        }
    }
}
