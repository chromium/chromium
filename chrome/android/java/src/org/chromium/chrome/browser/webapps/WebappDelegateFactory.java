// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.fullscreen.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_activity_glue.TabDelegateFactoryImpl;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link WebappActivity}.
 */
public class WebappDelegateFactory extends TabDelegateFactoryImpl {
    private static class WebappWebContentsDelegateAndroid
            extends ActivityTabWebContentsDelegateAndroid {
        private final WebappActivity mActivity;

        /** Action for do-nothing activity for activating WebAPK. */
        private static final String ACTION_ACTIVATE_WEBAPK =
                "org.chromium.chrome.browser.webapps.ActivateWebApkActivity.ACTIVATE";

        public WebappWebContentsDelegateAndroid(WebappActivity activity, Tab tab) {
            super(tab, activity);
            mActivity = activity;
        }

        @Override
        protected @WebDisplayMode int getDisplayMode() {
            return mActivity.getWebappInfo().displayMode();
        }

        @Override
        protected String getManifestScope() {
            return mActivity.getWebappInfo().scopeUrl();
        }

        @Override
        public void activateContents() {
            // Create an Intent that will be fired toward the WebappLauncherActivity, which in turn
            // will fire an Intent to launch the correct WebappActivity or WebApkActivity. On L+
            // this could probably be changed to call AppTask.moveToFront(), but for backwards
            // compatibility we relaunch it the hard way.
            String startUrl = mActivity.getWebappInfo().url();

            WebappInfo webappInfo = mActivity.getWebappInfo();
            if (webappInfo.isForWebApk()) {
                Intent activateIntent = null;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    activateIntent = new Intent(ACTION_ACTIVATE_WEBAPK);
                    activateIntent.setPackage(
                            ContextUtils.getApplicationContext().getPackageName());
                } else {
                    // For WebAPKs with new-style splash screen we cannot activate the WebAPK by
                    // sending an intent because that would relaunch the WebAPK.
                    assert !webappInfo.isSplashProvidedByWebApk();

                    activateIntent = WebApkNavigationClient.createLaunchWebApkIntent(
                            webappInfo.webApkPackageName(), startUrl, false /* forceNavigation */);
                }
                IntentUtils.safeStartActivity(mActivity, activateIntent);
                return;
            }

            Intent intent = new Intent();
            intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
            intent.setPackage(mActivity.getPackageName());
            mActivity.getWebappInfo().setWebappIntentExtras(intent);

            intent.putExtra(ShortcutHelper.EXTRA_MAC, ShortcutHelper.getEncodedMac(startUrl));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(ContextUtils.getApplicationContext(), intent);
        }

        @Override
        public boolean canShowAppBanners() {
            // Do not show banners when we are in a standalone activity.
            return false;
        }
    }

    private final WebappActivity mActivity;

    private TabWebContentsDelegateAndroid mWebContentsDelegateAndroid;

    public WebappDelegateFactory(WebappActivity activity) {
        super(activity);
        mActivity = activity;
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
        return new ChromeContextMenuPopulator(new TabContextMenuItemDelegate(tab),
                mActivity.getShareDelegate(), ChromeContextMenuPopulator.ContextMenuMode.WEB_APP);
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        mWebContentsDelegateAndroid = new WebappWebContentsDelegateAndroid(mActivity, tab);
        return mWebContentsDelegateAndroid;
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new WebappBrowserControlsDelegate(mActivity, tab),
                // Ensures browser controls hiding is delayed after activity start.
                mActivity.getFullscreenManager().getBrowserVisibilityDelegate());
    }

    WebContentsDelegateAndroid getWebContentsDelegate() {
        return mWebContentsDelegateAndroid;
    }
}
