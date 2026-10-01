// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;

/**
 * Keeps the client app alive, when possible, while CustomTabActivity is in foreground (see {@link
 * CustomTabsConnection#keepAliveForSession}).
 */
@NullMarked
public class CustomTabActivityClientConnectionKeeper implements StartStopWithNativeObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    public CustomTabActivityClientConnectionKeeper(
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mIntentDataProvider = intentDataProvider;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onStartWithNative() {
        CustomTabsConnection.getInstance()
                .keepAliveForSession(
                        mIntentDataProvider.getSession(),
                        mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStopWithNative() {
        CustomTabsConnection.getInstance()
                .dontKeepAliveForSession(mIntentDataProvider.getSession());
    }
}
