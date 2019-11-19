// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Displays error dialog on top of splash screen if there is a network error while loading the
 * start URL.
 */
public class WebApkSplashNetworkErrorObserver extends EmptyTabObserver {
    private Activity mActivity;
    private WebApkOfflineDialog mOfflineDialog;
    private String mWebApkName;

    private boolean mDidShowNetworkErrorDialog;

    /** Indicates whether reloading is allowed. */
    private boolean mAllowReloads;

    public WebApkSplashNetworkErrorObserver(Activity activity, String webApkName) {
        mActivity = activity;
        mWebApkName = webApkName;
    }

    public boolean isNetworkErrorDialogVisible() {
        return mOfflineDialog != null && mOfflineDialog.isShowing();
    }

    @Override
    public void onDidFinishNavigation(final Tab tab, NavigationHandle navigation) {
        if (!navigation.isInMainFrame()) return;

        switch (navigation.errorCode()) {
            case NetError.OK:
                if (mOfflineDialog != null) {
                    mOfflineDialog.cancel();
                    mOfflineDialog = null;
                }
                break;
            case NetError.ERR_NETWORK_CHANGED:
                onNetworkChanged(tab);
                break;
            default:
                onNetworkError(tab, navigation.errorCode());
                break;
        }
        WebApkUma.recordNetworkErrorWhenLaunch(-navigation.errorCode());
    }

    private void onNetworkChanged(Tab tab) {
        if (!mAllowReloads) return;

        // It is possible that we get {@link NetError.ERR_NETWORK_CHANGED} during the first
        // reload after the device is online. The navigation will fail until the next auto
        // reload fired by {@link NetErrorHelperCore}. We call reload explicitly to reduce the
        // waiting time.
        tab.reloadIgnoringCache();
        mAllowReloads = false;
    }

    private void onNetworkError(final Tab tab, @NetError int errorCode) {
        // Do not show the network error dialog more than once (e.g. if the user backed out of
        // the dialog).
        if (mDidShowNetworkErrorDialog) return;

        mDidShowNetworkErrorDialog = true;

        final NetworkChangeNotifier.ConnectionTypeObserver observer =
                new NetworkChangeNotifier.ConnectionTypeObserver() {
                    @Override
                    public void onConnectionTypeChanged(int connectionType) {
                        if (!NetworkChangeNotifier.isOnline()) return;

                        NetworkChangeNotifier.removeConnectionTypeObserver(this);
                        tab.reloadIgnoringCache();
                        // One more reload is allowed after the network connection is back.
                        mAllowReloads = true;
                    }
                };

        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mOfflineDialog = new WebApkOfflineDialog();
        mOfflineDialog.show(mActivity, generateNetworkErrorWebApkDialogMessage(errorCode));
    }

    /** Generates network error dialog message for the given error code. */
    private String generateNetworkErrorWebApkDialogMessage(@NetError int errorCode) {
        Context context = ContextUtils.getApplicationContext();
        switch (errorCode) {
            case NetError.ERR_INTERNET_DISCONNECTED:
                return context.getString(R.string.webapk_offline_dialog, mWebApkName);
            case NetError.ERR_TUNNEL_CONNECTION_FAILED:
                return context.getString(
                        R.string.webapk_network_error_message_tunnel_connection_failed);
            default:
                return context.getString(R.string.webapk_cannot_connect_to_site);
        }
    }
}
