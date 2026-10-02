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
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.WebContents;
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
     * @param scope The manifest scope of the WebAPK, as known at install time.
     */
    public void prepareIntentForReparenting(
            Intent intent, Tab tab, String webApkPackage, String url, String scope) {
        ThreadUtils.assertOnUiThread();
        clear();

        mIntentToken = new byte[32];
        mSecureRandom.nextBytes(mIntentToken);
        intent.putExtra(EXTRA_WEBAPK_REPARENT_TOKEN, mIntentToken);
        mTab = tab;
        mPackageName = webApkPackage;
        mUrl = url;
        mScope = scope;

        mTab.addObserver(mTabObserver);
    }

    /**
     * Validates the token and metadata from the incoming intent, returns the tab ID, and clears
     * state if reparenting succeeded.
     *
     * @param intent The intent received by Chrome from the WebAPK launcher bounce.
     * @return The tab ID to reparent, or {@link Tab#INVALID_TAB_ID} if invalid or not found.
     */
    public int detachAndRegisterTabAndClear(Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (mIntentToken == null || mTab == null) {
            return Tab.INVALID_TAB_ID;
        }
        byte[] tokenFromIntent =
                IntentUtils.safeGetByteArrayExtra(intent, EXTRA_WEBAPK_REPARENT_TOKEN);
        if (tokenFromIntent == null) {
            return Tab.INVALID_TAB_ID;
        }
        int result = Tab.INVALID_TAB_ID;

        String webApkPackage =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
        if (webApkPackage == null) {
            webApkPackage = intent.getPackage();
        }

        String url = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_URL);
        if (url == null) {
            url = IntentHandler.getUrlFromIntent(intent);
        }

        if (Arrays.equals(tokenFromIntent, mIntentToken)
                && (mPackageName == null || mPackageName.equals(webApkPackage))
                && (mUrl == null || mUrl.equals(url))
                && !mTab.isDestroyed()) {
            String scopeUrl = mScope;
            if (TextUtils.isEmpty(scopeUrl) && mUrl != null) {
                scopeUrl = ShortcutHelper.getScopeFromUrl(mUrl);
            }
            GURL tabUrl = mTab.getUrl();
            if (!GURL.isEmptyOrInvalid(tabUrl)
                    && !TextUtils.isEmpty(scopeUrl)
                    && UrlUtilities.isUrlWithinScope(tabUrl.getSpec(), scopeUrl)) {
                WebContents webContents = mTab.getWebContents();
                if (webContents != null) {
                    WebappsUtils.prunePreScopeNavigationHistory(webContents, new GURL(scopeUrl));
                }
                result = mTab.getId();
                AsyncTabParamsManagerSingleton.getInstance()
                        .add(result, new TabReparentingParams(mTab, null));

                // Detach tab from old window
                ReparentingTask.from(mTab).detach();
            }
            clear();
        }

        return result;
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
