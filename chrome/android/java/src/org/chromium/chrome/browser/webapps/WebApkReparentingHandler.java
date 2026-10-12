// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.security.SecureRandom;
import java.util.Arrays;

/**
 * Handles associating a randomly generated token with a tab being reparented into a WebAPK. The
 * WebAPK launcher forwards the token back to Chrome when bouncing into WebappLauncherActivity,
 * where Chrome validates it and unpacks the trusted tab ID.
 */
@NullMarked
public class WebApkReparentingHandler {
    /** Extra key to store the secure random token for tab reparenting. */
    public static final String EXTRA_WEBAPK_REPARENT_TOKEN =
            "org.chromium.chrome.browser.webapps.reparent_token";

    private static final Object INSTANCE_LOCK = new Object();
    private static @Nullable WebApkReparentingHandler sInstance;
    private final SecureRandom mSecureRandom = new SecureRandom();
    private final TabObserver mTabObserver =
            new TabObserver() {
                @Override
                public void onDestroyed(Tab tab) {
                    clear();
                }

                @Override
                public void onClosingStateChanged(Tab tab, boolean closing) {
                    if (closing) clear();
                }

                @Override
                public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                    if (window == null) clear();
                }

                @Override
                public void onCrash(Tab tab) {
                    clear();
                }
            };

    private byte @Nullable [] mIntentToken;
    private @Nullable Tab mTab;
    private @Nullable String mUrl;
    private @Nullable String mScope;
    private @Nullable String mPackageName;

    /** Get the singleton instance of this object. */
    public static WebApkReparentingHandler getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new WebApkReparentingHandler();
            }
        }
        return sInstance;
    }

    /**
     * Associates the originating tab with a random token attached to the launch intent.
     *
     * @param intent The launch intent sent to the WebAPK.
     * @param tab The tab to reparent.
     * @param webApkPackage The WebAPK package name.
     * @param url The start URL for the WebAPK.
     * @param scope The manifest scope of the WebAPK, as known at install time. If empty, the
     *     default scope derived from {@code url} is used.
     */
    public void prepareIntentForReparenting(
            Intent intent, Tab tab, String webApkPackage, String url, String scope) {
        ThreadUtils.assertOnUiThread();
        clear();
        // Don't attach a token for tabs that can't be reparented, so the WebAPK launches normally.
        if (!canReparentTab(tab)) return;

        mIntentToken = new byte[32];
        mSecureRandom.nextBytes(mIntentToken);
        intent.putExtra(EXTRA_WEBAPK_REPARENT_TOKEN, mIntentToken);
        mTab = tab;
        mPackageName = webApkPackage;
        mUrl = url;
        mScope = TextUtils.isEmpty(scope) ? ShortcutHelper.getScopeFromUrl(url) : scope;

        mTab.addObserver(mTabObserver);
    }

    /**
     * Validates the token and metadata from the incoming intent and returns the ID of the tab to
     * reparent, without detaching it yet. Clears the stored state if the token matches but the tab
     * can't be reparented, so the token remains one-shot.
     *
     * @param intent The intent received by Chrome from the WebAPK launcher bounce.
     * @return The tab ID to reparent, or {@link Tab#INVALID_TAB_ID} if invalid or not found.
     */
    public int getTabIdForReparenting(Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (mIntentToken == null || mTab == null) {
            return Tab.INVALID_TAB_ID;
        }
        byte[] tokenFromIntent =
                IntentUtils.safeGetByteArrayExtra(intent, EXTRA_WEBAPK_REPARENT_TOKEN);
        if (tokenFromIntent == null || !Arrays.equals(tokenFromIntent, mIntentToken)) {
            return Tab.INVALID_TAB_ID;
        }

        String webApkPackage =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
        if (webApkPackage == null) {
            webApkPackage = intent.getPackage();
        }

        String url = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_URL);
        if (url == null) {
            url = IntentHandler.getUrlFromIntent(intent);
        }

        // Set together with mIntentToken in prepareIntentForReparenting().
        assert mPackageName != null && mUrl != null && mScope != null;
        GURL tabUrl = mTab.getUrl();
        if (mPackageName.equals(webApkPackage)
                && mUrl.equals(url)
                && canReparentTab(mTab)
                && !GURL.isEmptyOrInvalid(tabUrl)
                && !TextUtils.isEmpty(mScope)
                && UrlUtilities.isUrlWithinScope(tabUrl.getSpec(), mScope)) {
            return mTab.getId();
        }
        clear();
        return Tab.INVALID_TAB_ID;
    }

    /**
     * Detaches the tab returned by {@link #getTabIdForReparenting(Intent)} so the WebappActivity
     * started for it can adopt it, and clears the stored state. Must only be called once that
     * WebappActivity has been started, in the same UI thread task.
     *
     * @param tabId The tab ID returned by {@link #getTabIdForReparenting(Intent)}.
     */
    public void detachAndRegisterTabAndClear(int tabId) {
        ThreadUtils.assertOnUiThread();
        Tab tab = mTab;
        String scopeUrl = mScope;
        // Validated by getTabIdForReparenting() in the same UI thread task.
        assert tab != null && scopeUrl != null && tab.getId() == tabId;
        // Clear before detaching, so mTabObserver isn't notified of the detach.
        clear();

        WebContents webContents = tab.getWebContents();
        // Checked by canReparentTab() in getTabIdForReparenting().
        assert webContents != null;
        WebappsUtils.prunePreScopeNavigationHistory(webContents, new GURL(scopeUrl));

        // Avoid leaving the originating browser window with zero tabs when reparenting its last
        // tab (matching Desktop Chrome's ReparentWebContentsIntoAppBrowser). Open the replacement
        // NTP before detaching so the TabModel never transiently drops to zero tabs.
        TabModel sourceTabModel = TabWindowManagerSingleton.getInstance().getTabModelForTab(tab);
        if (sourceTabModel != null && sourceTabModel.getCount() == 1) {
            TabCreatorUtil.launchNtp(sourceTabModel.getTabCreator());
        }

        AsyncTabParamsManagerSingleton.getInstance()
                .add(tabId, new TabReparentingParams(tab, null));

        // Detach tab from old window
        ReparentingTask.from(tab).detach();
    }

    /**
     * Returns whether {@code tab} can be adopted by WebappActivity as is. Only tabs in the main
     * browser app qualify: Custom Tab, TWA and WebappActivity tabs are managed by their own
     * activity (which can't host a replacement NTP), and WebappActivity always uses a regular
     * profile. Note that {@link Tab#isCustomTab()} is false for WebappActivity tabs, so {@link
     * Tab#isTabInBrowser()} is used instead. Since WebappActivity skips the initial load for a
     * reparented tab, the tab must also still be attached and loaded, rather than discarded,
     * crashed or showing an error page.
     */
    private static boolean canReparentTab(Tab tab) {
        return !tab.isDestroyed()
                && !tab.isClosing()
                && tab.isTabInBrowser()
                && !tab.isOffTheRecord()
                && !tab.isDetachedFromActivity()
                && !tab.isFrozen()
                && !tab.needsReload()
                && !tab.isShowingErrorPage()
                && !SadTab.isShowing(tab)
                && tab.getWebContents() != null;
    }

    /** Clears the stored token and metadata. */
    public void clear() {
        ThreadUtils.assertOnUiThread();
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
        }
        mIntentToken = null;
        mTab = null;
        mUrl = null;
        mScope = null;
        mPackageName = null;
    }
}
