// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The test opens new tabs")
@MediumTest
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InputTransferHandlerDelegateTest {

    private final String mLongHtmlTestPage =
            UrlUtils.encodeHtmlDataUri("<html><body style='height:100000px;'></body></html>");

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ActivityTabProvider mProvider;
    private InputTransferHandlerDelegate mDelegate;
    private RenderCoordinates mCoordinates;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(mLongHtmlTestPage);
        WebContents webContents = mActivityTestRule.getActivity().getActivityTab().getWebContents();
        mCoordinates = RenderCoordinates.fromWebContents(webContents);
        WebContentsUtils.reportAllFrameSubmissions(webContents, true);
        waitForViewportInitialization();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProvider = mActivityTestRule.getActivity().getActivityTabProvider();
                    mDelegate = new InputTransferHandlerDelegate(mProvider);
                });
    }

    private void waitForViewportInitialization() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mCoordinates.getContentHeightPixInt(), Matchers.greaterThan(10000));
                });
    }

    private void scrollPage() throws TimeoutException {
        ContentView contentView = mActivityTestRule.getActivity().getActivityTab().getContentView();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    contentView.scrollBy(/* x= */ 0, /* y= */ 400);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mDelegate.mScrollOffsetY, Matchers.greaterThan(0));
                });
    }

    @Test
    public void transfersInputAfterScroll() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mDelegate.canTransferInputToViz());
                });

        scrollPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mDelegate.canTransferInputToViz());
                });
    }

    @Test
    public void canObserveNewTabs() throws TimeoutException {
        scrollPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mDelegate.canTransferInputToViz());
                });

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                mLongHtmlTestPage,
                /* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mDelegate.canTransferInputToViz());
                });

        ChromeTabUtils.switchTabInCurrentTabModel(
                mActivityTestRule.getActivity(), /* tabIndex= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mDelegate.canTransferInputToViz());
                });
    }
}
