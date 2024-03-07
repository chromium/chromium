// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
public class PageInfoSharingControllerUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        PageInfoSharingControllerImpl.resetForTesting();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    public void testIsAvailable_withFeatureDisabled() {
        Tab tab = Mockito.mock(Tab.class);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNullTab() {
        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(null));
    }

    @Test
    public void testIsAvailable_withNullUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(null);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNonHttpUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_ABOUT);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withHttpUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        assertTrue(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_whileSharingAnotherTab() {
        Tab firstTab = Mockito.mock(Tab.class);
        when(firstTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        Tab secondTab = Mockito.mock(Tab.class);
        when(secondTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);

        ChromeOptionShareCallback optionShareCallback = mock(ChromeOptionShareCallback.class);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            optionShareCallback,
                                            firstTab);
                            assertFalse(
                                    "Page sharing process should only happen for one tab at a time",
                                    PageInfoSharingControllerImpl.getInstance()
                                            .isAvailableForTab(secondTab));
                        });
    }

    @Test
    public void testSharePageInfo_ensureSheetOpens() {
        ChromeOptionShareCallback optionShareCallback = mock(ChromeOptionShareCallback.class);
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            optionShareCallback,
                                            tab);
                            verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
                        });
    }
}
