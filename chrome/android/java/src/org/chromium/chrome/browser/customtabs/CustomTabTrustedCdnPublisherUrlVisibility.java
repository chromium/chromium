// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.UnownedUserData;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn.PublisherUrlVisibility;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.BooleanSupplier;

/**
 * Implementation of {@link TrustedCdn.PublisherUrlVisibility} to provide Tab with
 * the availability of publisher URL of trusted CDN when attached to a custom tab activity.
 */
class CustomTabTrustedCdnPublisherUrlVisibility
        implements PublisherUrlVisibility, DestroyObserver, UnownedUserData {
    private WindowAndroid mWindowAndroid;
    private BooleanSupplier mIsPublisherPackageForSession;

    CustomTabTrustedCdnPublisherUrlVisibility(
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BooleanSupplier isPublisherPackageForSession) {
        mWindowAndroid = windowAndroid;
        mIsPublisherPackageForSession = isPublisherPackageForSession;
        lifecycleDispatcher.register(this);
        PublisherUrlVisibility.attach(mWindowAndroid, this);
    }

    @Override
    public boolean canShowPublisherUrl(Tab tab) {
        return mIsPublisherPackageForSession.getAsBoolean();
    }

    @Override
    public void onDestroy() {
        PublisherUrlVisibility.detach(this);
        mWindowAndroid = null;
        mIsPublisherPackageForSession = null;
    }
}
