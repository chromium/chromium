// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link HistoryFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoryFeaturesUnitTest {
    @Test
    @DisableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
    public void testIsCriticalActionChatLinkoutsEnabled_featureDisabled() {
        assertFalse(HistoryFeatures.isCriticalActionChatLinkoutsEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
    public void testIsCriticalActionChatLinkoutsEnabled_featureEnabledDefaultParams() {
        assertTrue(HistoryFeatures.isCriticalActionChatLinkoutsEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY + ":enable_chat_linkouts/false")
    public void testIsCriticalActionChatLinkoutsEnabled_paramDisabled() {
        assertFalse(HistoryFeatures.isCriticalActionChatLinkoutsEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY + ":enable_chat_linkouts/true")
    public void testIsCriticalActionChatLinkoutsEnabled_paramEnabled() {
        assertTrue(HistoryFeatures.isCriticalActionChatLinkoutsEnabled());
    }
}
