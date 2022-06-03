// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * A utility class for mocking AndroidSyncSettings.
 */
public class AndroidSyncSettingsTestUtils {
    /**
     * Sets up a MockSyncContentResolverDelegate with Android's master sync setting enabled.
     * (This setting is found, e.g., under Settings > Accounts & Sync > Auto-sync data.)
     */
    @CalledByNative
    @VisibleForTesting
    public static void setUpAndroidSyncSettingsForTesting() {
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        delegate.setMasterSyncAutomatically(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> SyncContentResolverDelegate.overrideForTests(delegate));
    }

    public static boolean getIsChromeSyncEnabledOnUiThread() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    public static boolean getDoesMasterSyncAllowSyncOnUiThread() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> AndroidSyncSettings.get().doesMasterSyncSettingAllowChromeSync());
    }
}
