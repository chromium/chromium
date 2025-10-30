// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.app.tabmodel.HeadlessTabDelegateFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;

/** Used by tab_android_unittest.cc to initialize a tab. */
public class TabAndroidTestHelper {
    private TabAndroidTestHelper() {}

    /**
     * Price tracking has a TabHelper that registers and expects native flags to exist. This is not
     * present in C++ unit tests so just disable the feature.
     */
    private static void disablePriceTracking() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
    }

    /** Creates and initializes a tab for C++ unit test usage. */
    @CalledByNative
    public static TabImpl createAndInitializeTabImpl(
            @TabId int tabId, Profile profile, @TabLaunchType int tabLaunchType) {
        disablePriceTracking();

        // Create a frozen tab.
        TabImpl tab = new TabImpl(tabId, profile, tabLaunchType, /* isArchived= */ false);
        tab.initialize(
                /* parent= */ null,
                /* creationState= */ null,
                new LoadUrlParams("about:blank"),
                /* pendingTitle= */ null,
                /* webContents= */ null,
                new HeadlessTabDelegateFactory(),
                /* initiallyHidden= */ true,
                /* tabState= */ null,
                /* initializeRenderer= */ false,
                /* isPinned= */ false);
        return tab;
    }
}
