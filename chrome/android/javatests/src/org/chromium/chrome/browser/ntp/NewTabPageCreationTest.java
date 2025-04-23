// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Tests for creating a tab with NewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class NewTabPageCreationTest {
    private static final String TEST_URL = "/chrome/test/data/android/test.html";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private NewTabPageCreationState mTestState;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() {
        mTestState = spy(new NewTabPageCreationState());
        NewTabPageCreationState.setInstanceForTesting(mTestState);

        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE)
    public void testCreateNTPInNewTab() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("NewTabPage.OpenedInNewTab", 2 /* FROM_CHROME_UI */)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getCurrentTabCreator().launchNtp();
                });

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        verify(mTestState).onNewTabCreated();
        verify(mTestState).onNtpLoaded(any(NewTabPageManager.class));

        mOmnibox.checkFocus(true);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE)
    public void testCreateNTPInExistingTab() throws Exception {
        String testUrl = mActivityTestRule.getTestServer().getURL(TEST_URL);
        mActivityTestRule.loadUrlInNewTab(testUrl);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        assertNull(tab.getNativePage());
        assertEquals(tab.getUrl().getSpec(), testUrl);

        new TabLoadObserver(tab).fullyLoadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        verify(mTestState, never()).onNewTabCreated();
        verify(mTestState).onNtpLoaded(any(NewTabPageManager.class));

        mOmnibox.checkFocus(false);
    }
}
