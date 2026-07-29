// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Instrumentation test verifying tab strip spinner behavior on background navigation failure. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_STOP_SPINNER_ON_LOAD_STOP)
public class TabStripNavigationFailureTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private StripLayoutHelper mStripLayoutHelper;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);
    }

    @Test
    @MediumTest
    public void testBackgroundTabNavigationFailureLeavesSpinnerStuck() throws Exception {
        CallbackHelper onPageLoadFailedHelper = new CallbackHelper();
        final Tab[] bgTabHolder = new Tab[1];

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    tabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void didAddTab(
                                        Tab tab,
                                        @TabLaunchType int type,
                                        @TabCreationState int creationState,
                                        boolean markedForSelection) {
                                    bgTabHolder[0] = tab;
                                    tab.addObserver(
                                            new EmptyTabObserver() {
                                                @Override
                                                public void onPageLoadFailed(
                                                        Tab tab, int errorCode) {
                                                    onPageLoadFailedHelper.notifyCalled();
                                                }
                                            });
                                }
                            });
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabCreator()
                            .createNewTab(
                                    new LoadUrlParams(mTestServer.getURL("/close-socket")),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    mActivityTestRule.getActivity().getActivityTab());
                });

        onPageLoadFailedHelper.waitForCallback(0);

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab bgTab = bgTabHolder[0];
                    Criteria.checkThat(
                            "Background tab should be created", bgTab, Matchers.notNullValue());
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "StripLayoutTab should not be loading on navigation failure",
                            stripTab.isLoading(),
                            Matchers.is(false));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testSameDocumentLoadStoppedIgnoredLeavesSpinnerStuck() throws Exception {
        final Tab[] bgTabHolder = new Tab[1];

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    tabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void didAddTab(
                                        Tab tab,
                                        @TabLaunchType int type,
                                        @TabCreationState int creationState,
                                        boolean markedForSelection) {
                                    bgTabHolder[0] = tab;
                                }
                            });
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabCreator()
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    mActivityTestRule.getActivity().getActivityTab());
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab bgTab = bgTabHolder[0];
                    Criteria.checkThat(
                            "Background tab should be created", bgTab, Matchers.notNullValue());
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        Tab bgTab = bgTabHolder[0];
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Background tab should finish initial load",
                            bgTab.isLoading(),
                            Matchers.is(false));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.loadUrlOnUiThread(bgTab, mTestServer.getURL("/slow?10"));

        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "StripLayoutTab should be loading",
                            stripTab.isLoading(),
                            Matchers.is(true));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ObserverList.RewindableIterator<TabObserver> observers =
                            TabTestUtils.getTabObservers(bgTab);
                    while (observers.hasNext()) {
                        observers.next().onLoadStopped(bgTab, /* toDifferentDocument= */ false);
                    }
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "StripLayoutTab should stop loading after same-document load stopped",
                            stripTab.isLoading(),
                            Matchers.is(false));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testContinuousSubframeReloadsLeaveSpinnerStuck() throws Exception {
        final Tab[] bgTabHolder = new Tab[1];

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    tabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void didAddTab(
                                        Tab tab,
                                        @TabLaunchType int type,
                                        @TabCreationState int creationState,
                                        boolean markedForSelection) {
                                    bgTabHolder[0] = tab;
                                }
                            });
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabCreator()
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    mActivityTestRule.getActivity().getActivityTab());
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab bgTab = bgTabHolder[0];
                    Criteria.checkThat(
                            "Background tab should be created", bgTab, Matchers.notNullValue());
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        Tab bgTab = bgTabHolder[0];
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Background tab should finish initial load",
                            bgTab.isLoading(),
                            Matchers.is(false));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        String testUrl = mTestServer.getURL("/iframe?" + mTestServer.getURL("/hung"));

        ChromeTabUtils.loadUrlOnUiThread(bgTab, testUrl);

        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "StripLayoutTab should be loading",
                            stripTab.isLoading(),
                            Matchers.is(true));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutTab stripTab =
                            TabStripUtils.findStripLayoutTab(
                                    mActivityTestRule.getActivity(), false, bgTab.getId());
                    Criteria.checkThat(
                            "StripLayoutTab should exist", stripTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "StripLayoutTab should stop loading after main document completion"
                                    + " despite subframe activity",
                            stripTab.isLoading(),
                            Matchers.is(false));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
