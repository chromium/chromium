// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayDeque;

/**
 * Test suite for ContentView focus and its interaction with Tab switcher,
 * Tab swiping, etc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContentViewFocusTest {
    @Rule
    public ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    private static final int WAIT_RESPONSE_MS = 2000;

    private final ArrayDeque<Boolean> mFocusChanges = new ArrayDeque<Boolean>();

    private String mTitle;

    private void addFocusChangedListener(View view) {
        view.setOnFocusChangeListener((v, hasFocus) -> {
            synchronized (mFocusChanges) {
                mFocusChanges.add(Boolean.valueOf(hasFocus));
                mFocusChanges.notify();
            }
        });
    }

    private boolean blockForFocusChanged() throws InterruptedException  {
        long endTime = System.currentTimeMillis() + WAIT_RESPONSE_MS * 2;
        synchronized (mFocusChanges) {
            while (true) {
                if (!mFocusChanges.isEmpty()) {
                    return mFocusChanges.removeFirst();
                }
                long sleepTime = endTime - System.currentTimeMillis();
                if (sleepTime <= 0) {
                    throw new RuntimeException("Didn't get event");
                }
                mFocusChanges.wait(sleepTime);
            }
        }
    }

    private boolean haveFocusChanges() {
        synchronized (mFocusChanges) {
            return !mFocusChanges.isEmpty();
        }
    }

    /**
     * Verify ContentView loses/gains focus on swiping tab.
     *
     * @throws Exception
     * @MediumTest
     * @Feature({"TabContents"})
     * @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
     */
    @Test
    @FlakyTest(message = "http://crbug.com/172473")
    public void testHideSelectionOnPhoneTabSwiping() throws Exception {
        mChromeTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Setup
        ChromeTabUtils.newTabsFromMenu(InstrumentationRegistry.getInstrumentation(),
                mChromeTabbedActivityTestRule.getActivity(), 2);
        String url = UrlUtils.getIsolatedTestFileUrl(
                "chrome/test/data/android/content_view_focus/content_view_focus_long_text.html");
        mChromeTabbedActivityTestRule.loadUrl(url);
        View view = mChromeTabbedActivityTestRule.getActivity().getActivityTab().getContentView();

        // Give the content view focus
        TestTouchUtils.longClickView(InstrumentationRegistry.getInstrumentation(), view, 50, 10);
        Assert.assertTrue("ContentView is focused", view.hasFocus());

        // Start the swipe
        addFocusChangedListener(view);
        final EdgeSwipeHandler edgeSwipeHandler = mChromeTabbedActivityTestRule.getActivity()
                                                          .getLayoutManager()
                                                          .getToolbarSwipeHandler();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            edgeSwipeHandler.swipeStarted(ScrollDirection.RIGHT, 0, 0);
            edgeSwipeHandler.swipeUpdated(100, 0, 100, 0, 100, 0);
        });

        CriteriaHelper.pollUiThread(
                new Criteria("Layout still requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver =
                                mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
                        return !driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        Assert.assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // End the drag
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> edgeSwipeHandler.swipeFinished());

        CriteriaHelper.pollUiThread(
                new Criteria("Layout not requesting Tab Android view be attached") {
                    @Override
                    public boolean isSatisfied() {
                        LayoutManager driver =
                                mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
                        return driver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                });

        Assert.assertTrue("Content view didn't regain focus", blockForFocusChanged());
        Assert.assertFalse("Unexpected focus change", haveFocusChanges());
    }

    /**
     * Verify ContentView loses/gains focus on overview mode.
     *
     * @throws Exception
     * @Feature({"TabContents"})
     */
    @Test
    @MediumTest
    @Feature({"TabContents"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @FlakyTest(message = "http://crbug.com/967128")
    public void testHideSelectionOnPhoneTabSwitcher() throws Exception {
        mChromeTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Setup
        OverviewModeBehaviorWatcher showWatcher = new OverviewModeBehaviorWatcher(
                mChromeTabbedActivityTestRule.getActivity().getLayoutManager(), true, false);
        OverviewModeBehaviorWatcher hideWatcher = new OverviewModeBehaviorWatcher(
                mChromeTabbedActivityTestRule.getActivity().getLayoutManager(), false, true);
        View currentView =
                mChromeTabbedActivityTestRule.getActivity().getActivityTab().getContentView();
        addFocusChangedListener(currentView);

        // Enter the tab switcher
        View tabSwitcherButton =
                mChromeTabbedActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        TouchCommon.singleClickView(
                mChromeTabbedActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        showWatcher.waitForBehavior();

        // Make sure the view loses focus. It is immediately given focus back
        // because it's the only focusable view.
        Assert.assertFalse("Content view didn't lose focus", blockForFocusChanged());

        // Hide the tab switcher
        tabSwitcherButton =
                mChromeTabbedActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found.", tabSwitcherButton);
        TouchCommon.singleClickView(
                mChromeTabbedActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        hideWatcher.waitForBehavior();

        Assert.assertTrue("Content view didn't regain focus", blockForFocusChanged());
        Assert.assertFalse("Unexpected focus change", haveFocusChanges());
    }

    /**
     * Verify ContentView window focus changes propagate to contents.
     *
     * @throws Exception
     */
    @Test
    @MediumTest
    public void testPauseTriggersBlur() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final WebContents webContents = mActivityTestRule.getWebContents();
        final CallbackHelper onTitleUpdatedHelper = new CallbackHelper();
        final WebContentsObserver observer =
                new WebContentsObserver(webContents) {
                    @Override
                    public void titleWasSet(String title) {
                        mTitle = title;
                        onTitleUpdatedHelper.notifyCalled();
                    }
                };
        int callCount = onTitleUpdatedHelper.getCallCount();
        String url = UrlUtils.getIsolatedTestFileUrl(
                "chrome/test/data/android/content_view_focus/content_view_blur_focus.html");
        mActivityTestRule.loadUrl(url);
        ViewEventSink eventSink = WebContentsUtils.getViewEventSink(webContents);
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("initial", mTitle);
        callCount = onTitleUpdatedHelper.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> eventSink.onPauseForTesting());
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("blurred", mTitle);
        callCount = onTitleUpdatedHelper.getCallCount();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> eventSink.onResumeForTesting());
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("focused", mTitle);
        mActivityTestRule.getWebContents().removeObserver(observer);
    }
}
