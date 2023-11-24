// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.os.Bundle;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Provides the client package name for TWAs - this can come from either the Custom Tabs Connection
 * or one previously stored in the Activity's save instance state.
 */
@ActivityScope
public class ClientPackageNameProvider implements SaveInstanceStateObserver {
    /** Key for storing in Activity instance state. */
    private static final String KEY_CLIENT_PACKAGE = "twaClientPackageName";

    private final String mClientPackageName;

    @Inject
    public ClientPackageNameProvider(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier) {
        Bundle savedInstanceState = savedInstanceStateSupplier.get();
        if (savedInstanceState != null) {
            mClientPackageName = savedInstanceState.getString(KEY_CLIENT_PACKAGE);
        } else {
            mClientPackageName =
                    customTabsConnection.getClientPackageNameForSession(
                            intentDataProvider.getSession());
        }
        assert mClientPackageName != null;

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
