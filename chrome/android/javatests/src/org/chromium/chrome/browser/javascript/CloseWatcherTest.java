// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.javascript;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for CloseWatcher's ability to receive signals from the
 * system back button.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-experimental-web-platform-features", "enable-features=CloseWatcher"})
public class CloseWatcherTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_URL =
            UrlUtils.encodeHtmlDataUri("<body><script>let watcher = new CloseWatcher(); "
                    + "watcher.onclose = () => window.document.title = 'SUCCESS';</script></body>");

    private Tab mTab;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getActivityTab());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackButtonTriggersCloseWatcher() throws Throwable {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> activity.onBackPressed());
        new TabTitleObserver(mTab, "SUCCESS").waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackButtonTriggersCloseWatcher_BackGestureRefactor() throws Throwable {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> activity.onBackPressed());
        new TabTitleObserver(mTab, "SUCCESS").waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackButtonClosesDialogElement() throws Throwable {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("<dialog id=mydialog>hello</dialog>"
                + "<script>mydialog.showModal();"
                + "mydialog.onclose = () => window.document.title = 'SUCCESS';</script>"));
        TestThreadUtils.runOnUiThreadBlocking(() -> activity.onBackPressed());
        new TabTitleObserver(mTab, "SUCCESS").waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackButtonClosesDialogElement_BackGestureRefactor() throws Throwable {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("<dialog id=mydialog>hello</dialog>"
                + "<script>mydialog.showModal();"
                + "mydialog.onclose = () => window.document.title = 'SUCCESS';</script>"));
        TestThreadUtils.runOnUiThreadBlocking(() -> activity.onBackPressed());
        new TabTitleObserver(mTab, "SUCCESS").waitForTitleUpdate(3);
    }
}
