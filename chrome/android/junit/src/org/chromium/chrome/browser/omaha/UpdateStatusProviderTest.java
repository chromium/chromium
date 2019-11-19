// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_AVAILABLE;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_DOWNLOADING;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_FAILED;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_READY;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.NONE;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Unit tests for UpdateStatusProvider.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class UpdateStatusProviderTest {
    private static void verify(@UpdateStatusProvider.UpdateState int expected,
            @UpdateConfigs.UpdateFlowConfiguration int configuration,
            @UpdateStatusProvider.UpdateState int omahaState,
            @UpdateStatusProvider.UpdateState int inlineState) {
        @UpdateStatusProvider.UpdateState
        int result = UpdateStatusProvider.resolveOmahaAndInlineStatus(
                configuration, omahaState, inlineState);
        Assert.assertEquals("{config=" + configuration + ", omaha=" + omahaState
                        + ", inline=" + inlineState + "}",
                expected, result);
    }

    @Test
    @Feature("omaha")
    public void testNeverShow() {
        @UpdateConfigs.UpdateFlowConfiguration
        int config = UpdateConfigs.UpdateFlowConfiguration.NEVER_SHOW;
        verify(NONE, config, NONE, NONE);
        verify(NONE, config, NONE, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, NONE, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, NONE, INLINE_UPDATE_READY);
        verify(NONE, config, NONE, INLINE_UPDATE_FAILED);
        verify(NONE, config, UNSUPPORTED_OS_VERSION, NONE);
        verify(NONE, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_READY);
        verify(NONE, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_FAILED);
        verify(NONE, config, UPDATE_AVAILABLE, NONE);
        verify(NONE, config, UPDATE_AVAILABLE, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, UPDATE_AVAILABLE, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, UPDATE_AVAILABLE, INLINE_UPDATE_READY);
        verify(NONE, config, UPDATE_AVAILABLE, INLINE_UPDATE_FAILED);
    }

    @Test
    @Feature("omaha")
    public void testInlineOnly() {
        @UpdateConfigs.UpdateFlowConfiguration
        int config = UpdateConfigs.UpdateFlowConfiguration.INLINE_ONLY;
        verify(NONE, config, NONE, NONE);
        verify(NONE, config, NONE, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, NONE, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, NONE, INLINE_UPDATE_READY);
        verify(NONE, config, NONE, INLINE_UPDATE_FAILED);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, NONE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_AVAILABLE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_DOWNLOADING);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_READY);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_FAILED);
        verify(NONE, config, UPDATE_AVAILABLE, NONE);
        verify(INLINE_UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_AVAILABLE);
        verify(INLINE_UPDATE_DOWNLOADING, config, UPDATE_AVAILABLE, INLINE_UPDATE_DOWNLOADING);
        verify(INLINE_UPDATE_READY, config, UPDATE_AVAILABLE, INLINE_UPDATE_READY);
        verify(INLINE_UPDATE_FAILED, config, UPDATE_AVAILABLE, INLINE_UPDATE_FAILED);
    }

    @Test
    @Feature("omaha")
    public void testIntentOnly() {
        @UpdateConfigs.UpdateFlowConfiguration
        int config = UpdateConfigs.UpdateFlowConfiguration.INTENT_ONLY;
        verify(NONE, config, NONE, NONE);
        verify(NONE, config, NONE, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, NONE, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, NONE, INLINE_UPDATE_READY);
        verify(NONE, config, NONE, INLINE_UPDATE_FAILED);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, NONE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_AVAILABLE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_DOWNLOADING);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_READY);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_FAILED);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, NONE);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_AVAILABLE);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_DOWNLOADING);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_READY);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_FAILED);
    }

    @Test
    @Feature("omaha")
    public void testBestEffort() {
        @UpdateConfigs.UpdateFlowConfiguration
        int config = UpdateConfigs.UpdateFlowConfiguration.BEST_EFFORT;
        verify(NONE, config, NONE, NONE);
        verify(NONE, config, NONE, INLINE_UPDATE_AVAILABLE);
        verify(NONE, config, NONE, INLINE_UPDATE_DOWNLOADING);
        verify(NONE, config, NONE, INLINE_UPDATE_READY);
        verify(NONE, config, NONE, INLINE_UPDATE_FAILED);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, NONE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_AVAILABLE);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_DOWNLOADING);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_READY);
        verify(UNSUPPORTED_OS_VERSION, config, UNSUPPORTED_OS_VERSION, INLINE_UPDATE_FAILED);
        verify(UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, NONE);
        verify(INLINE_UPDATE_AVAILABLE, config, UPDATE_AVAILABLE, INLINE_UPDATE_AVAILABLE);
        verify(INLINE_UPDATE_DOWNLOADING, config, UPDATE_AVAILABLE, INLINE_UPDATE_DOWNLOADING);
        verify(INLINE_UPDATE_READY, config, UPDATE_AVAILABLE, INLINE_UPDATE_READY);
        verify(INLINE_UPDATE_FAILED, config, UPDATE_AVAILABLE, INLINE_UPDATE_FAILED);
    }
}
