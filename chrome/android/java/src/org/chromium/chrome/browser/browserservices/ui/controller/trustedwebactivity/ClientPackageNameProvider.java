// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import android.os.Bundle;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;

/**
 * Provides the client package name for TWAs - this can come from either the Custom Tabs Connection
 * or one previously stored in the Activity's save instance state.
 */
public class ClientPackageNameProvider implements SaveInstanceStateObserver {
    /** Key for storing in Activity instance state. */
    private static final String KEY_CLIENT_PACKAGE = "twaClientPackageName";

    private final String mClientPackageName;

    public ClientPackageNameProvider(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            mClientPackageName = savedInstanceState.getString(KEY_CLIENT_PACKAGE);
        } else {
            mClientPackageName =
                    CustomTabsConnection.getInstance()
                            .getClientPackageNameForSession(intentDataProvider.getSession());
        }

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        // TODO(pshmakov): address this problem in a more general way, http://crbug.com/952221
        outState.putString(KEY_CLIENT_PACKAGE, mClientPackageName);
    }

    public String get() {
        return mClientPackageName;
    }
}
