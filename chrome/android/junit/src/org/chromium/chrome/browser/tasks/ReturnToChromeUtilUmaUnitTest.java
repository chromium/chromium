// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Unit tests for {@link ReturnToChromeUtil} class in order to test whether the user actions
 * are recorded successfully in histogram.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReturnToChromeUtilUmaUnitTest {
    private static final String TEST_URL = "https://www.example.com/";
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";
    private static final int PAGE_TRANSITION_GENERATED_BEFORE_MASK = 33554437;
    private static final int PAGE_TRANSITION_TYPED_BEFORE_MASK = 33554433;

    @Mock
    private ChromeActivity mMockChromeActivity;

    @Mock
    private TabCreator mTabCreator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockChromeActivity.getTabCreator(false)).thenReturn(mTabCreator);
        ReturnToChromeUtil.setActivityPresentingOverivewWithOmniboxForTesting(mMockChromeActivity);
    }

    @Test
    @SmallTest
    public void testRecordHistogramOmniboxClick_StartSurface() {
        // Test searching using omnibox.
        ReturnToChromeUtil.handleLoadUrlWithPostDataFromStartSurface(
                new LoadUrlParams(TEST_URL, PAGE_TRANSITION_GENERATED_BEFORE_MASK), null, null,
                false, null);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded "
                        + "correctly when doing search using omnibox.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.OMNIBOX));

        // Test navigating using omnibox.
        ReturnToChromeUtil.handleLoadUrlWithPostDataFromStartSurface(
                new LoadUrlParams(TEST_URL, PAGE_TRANSITION_TYPED_BEFORE_MASK), null, null, false,
                null);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded "
                        + "correctly when navigating using omnibox.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.OMNIBOX));

        // Test clicking on MV tiles.
        ReturnToChromeUtil.handleLoadUrlFromStartSurface(
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, false, null);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " shouldn't be "
                        + "recorded when click on MV tiles.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.OMNIBOX));
    }
}