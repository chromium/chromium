// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.base.test.transit.Condition.awaiting;
import static org.chromium.base.test.transit.Condition.whether;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TriState;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.PageLoadedCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_strip.TabStripFacility;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.function.Function;
import java.util.function.Supplier;

/** Instrumentation test verifying tab strip spinner behavior on background navigation failure. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@EnableFeatures(ChromeFeatureList.TAB_STRIP_STOP_SPINNER_ON_LOAD_STOP)
@DisableIf.Device(DeviceFormFactor.AUTOMOTIVE) // crbug.com/462592111
public class TabStripNavigationFailureTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;
    private TabStripFacility<CtaPageStation> mTabStrip;

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
        mTabStrip = mPage.expectTabStrip();
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
                                            new TabObserver() {
                                                @Override
                                                public void onPageLoadFailed(
                                                        Tab tab, int errorCode) {
                                                    onPageLoadFailedHelper.notifyCalled();
                                                }
                                            });
                                }
                            });
                });

        createBackgroundTabTo(mTestServer.getURL("/close-socket"))
                .waitFor(
                        SimpleConditions.instrumentationThreadCondition(
                                "Background tab navigation failed",
                                () -> whether(onPageLoadFailedHelper.getCallCount() > 0)),
                        stripTabLoadingCondition(() -> bgTabHolder[0], /* loading= */ false));
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
                });

        createBackgroundTabTo("about:blank")
                .waitForAnd(
                        new PageLoadedCondition(() -> bgTabHolder[0], TriState.FALSE),
                        stripTabLoadingCondition(() -> bgTabHolder[0], /* loading= */ false))
                .reach(mTabStrip.settled());

        Tab bgTab = bgTabHolder[0];

        mPage.runTo(() -> ChromeTabUtils.loadUrlOnUiThread(bgTab, mTestServer.getURL("/slow?10")))
                .waitFor(stripTabLoadingCondition(() -> bgTab, /* loading= */ true));

        mPage.runOnUiThreadTo(
                        () -> {
                            for (TabObserver observer : TabTestUtils.getTabObservers(bgTab)) {
                                observer.onLoadStopped(bgTab, /* toDifferentDocument= */ false);
                            }
                        })
                .waitFor(stripTabLoadingCondition(() -> bgTab, /* loading= */ false));
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
                });

        createBackgroundTabTo("about:blank")
                .waitForAnd(
                        new PageLoadedCondition(() -> bgTabHolder[0], TriState.FALSE),
                        stripTabLoadingCondition(() -> bgTabHolder[0], /* loading= */ false))
                .reach(mTabStrip.settled());

        Tab bgTab = bgTabHolder[0];

        String testUrl = mTestServer.getURL("/iframe?" + mTestServer.getURL("/hung"));

        mPage.runTo(() -> ChromeTabUtils.loadUrlOnUiThread(bgTab, testUrl))
                .waitForAnd(stripTabLoadingCondition(() -> bgTab, /* loading= */ true))
                .reach(mTabStrip.settled());

        mPage.noopTo()
                .withTimeout(10000L)
                .waitFor(stripTabLoadingCondition(() -> bgTab, /* loading= */ false));
    }

    /** Starts a Trip creating a background tab that loads {@code url}. */
    private TripBuilder createBackgroundTabTo(String url) {
        return mPage.runOnUiThreadTo(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getCurrentTabCreator()
                                .createNewTab(
                                        new LoadUrlParams(url),
                                        TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                        mActivityTestRule.getActivityTab()));
    }

    /** Fulfilled when the tab is on the regular strip and shows a loading spinner or not. */
    private Condition stripTabLoadingCondition(Supplier<Tab> tabSupplier, boolean loading) {
        return stripTabCondition(
                "Tab on the strip with isLoading=" + loading,
                tabSupplier,
                stripTab ->
                        whether(
                                stripTab.isLoading() == loading,
                                "isLoading=%b",
                                stripTab.isLoading()));
    }

    private Condition stripTabCondition(
            String description,
            Supplier<Tab> tabSupplier,
            Function<StripLayoutTab, ConditionStatus> check) {
        return SimpleConditions.uiThreadCondition(
                description,
                mTabStrip.stripLayoutHelperManagerElement,
                manager -> {
                    Tab tab = tabSupplier.get();
                    if (tab == null) {
                        return awaiting("Tab not created");
                    }
                    StripLayoutTab stripTab =
                            manager.getStripLayoutHelper(/* incognito= */ false)
                                    .findTabById(tab.getId());
                    if (stripTab == null) {
                        return awaiting("Not on the strip");
                    }
                    return check.apply(stripTab);
                });
    }
}
