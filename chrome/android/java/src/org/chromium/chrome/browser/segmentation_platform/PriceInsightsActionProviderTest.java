// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.Mockito.doReturn;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link PriceInsightsActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceInsightsActionProviderTest {

    @Mock private Tab mMockTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testPriceInsightsDisabledForNonHttpUrls() {
        // Use a non-http(s) url (about:blank).
        doReturn(JUnitTestGURLs.ABOUT_BLANK).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider = new PriceInsightsActionProvider();
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceInsights());
    }
}
