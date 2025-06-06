// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.junit.Assert.assertFalse;

import android.os.Handler;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

import java.util.Map;

/** Unit tests for {@link TabGroupingActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupingActionProviderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    @Test
    public void testGetAction() {
        var provider = new TabGroupingActionProvider();
        var signalAccumulator =
                new SignalAccumulator(
                        new Handler(),
                        mTab,
                        Map.of(AdaptiveToolbarButtonVariant.TAB_GROUPING, provider));
        provider.getAction(mTab, signalAccumulator);

        assertFalse(signalAccumulator.getSignal(AdaptiveToolbarButtonVariant.TAB_GROUPING));
    }
}
