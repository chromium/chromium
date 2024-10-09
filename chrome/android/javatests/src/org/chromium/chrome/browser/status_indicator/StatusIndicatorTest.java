// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Integration tests for status indicator covering related code in {@link
 * StatusIndicatorCoordinator} and {@link TabbedRootUiCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/40112282): Enable for tablets once we support them.
@Restriction({DeviceFormFactor.PHONE})
public class StatusIndicatorTest {

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorSceneLayer mStatusIndicatorSceneLayer;
    private View mControlContainer;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Before
    public void setUp() throws InterruptedException {
        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        mStatusIndicatorCoordinator =
                ((TabbedRootUiCoordinator)
                                mActivityTestRule.getActivity().getRootUiCoordinatorForTesting())
                        .getStatusIndicatorCoordinatorForTesting();
        mStatusIndicatorSceneLayer = mStatusIndicatorCoordinator.getSceneLayer();
        mControlContainer = mActivityTestRule.getActivity().findViewById(R.id.control_container);
        mBrowserControlsStateProvider = mActivityTestRule.getActivity().getBrowserControlsManager();
    }

    @Test
    @MediumTest
    public void testInitialState() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertNull(
                "Status indicator shouldn't be in the hierarchy initially.", getStatusIndicator());
        Assert.assertNotNull(
                "Status indicator stub should be in the hierarchy initially.",
                mActivityTestRule.getActivity().findViewById(R.id.status_indicator_stub));
        Assert.assertFalse(
                "Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        Assert.assertEquals(
                "Wrong initial control container top margin.",
                0,
                getTopMarginOf(mControlContainer));
    }

    @Test
    @MediumTest
    public void testShowAndHide() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStatusIndicatorCoordinator.show(
                                "Status", null, Color.BLACK, Color.WHITE, Color.WHITE));

        // Wait until the status indicator finishes animating, or becomes fully visible.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(getStatusIndicator().getHeight()));
                    Criteria.checkThat(
                            getStatusIndicator().getVisibility(), Matchers.is(View.VISIBLE));
                });

        Assert.assertEquals(
                "Wrong background color.",
                Color.BLACK,
                ((ColorDrawable) getStatusIndicator().getBackground()).getColor());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStatusIndicatorCoordinator.updateContent(
                                "Exit status",
                                null,
                                Color.WHITE,
                                Color.BLACK,
                                Color.BLACK,
                                CallbackUtils.emptyRunnable()));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The Android view should be visible.
        Assert.assertEquals(
                "Wrong Android view visibility.",
                View.VISIBLE,
                getStatusIndicator().getVisibility());
        Assert.assertEquals(
                "Wrong background color.",
                Color.WHITE,
                ((ColorDrawable) getStatusIndicator().getBackground()).getColor());

        ThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::hide);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(0));
                    Criteria.checkThat(
                            getStatusIndicator().getVisibility(), Matchers.is(View.GONE));
                });

        Assert.assertFalse(
                "Composited view shouldn't be visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
    }

    @Test
    @MediumTest
    public void testShowAfterHide() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStatusIndicatorCoordinator.show(
                                "Status", null, Color.BLACK, Color.WHITE, Color.WHITE));

        // Wait until the status indicator finishes animating, or becomes fully visible.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(getStatusIndicator().getHeight()));
                    Criteria.checkThat(
                            getStatusIndicator().getVisibility(), Matchers.is(View.VISIBLE));
                });

        ThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::hide);

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(0));
                    Criteria.checkThat(
                            getStatusIndicator().getVisibility(), Matchers.is(View.GONE));
                });

        Assert.assertFalse(
                "Composited view shouldn't be visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mStatusIndicatorCoordinator.show(
                                "Status", null, Color.BLACK, Color.WHITE, Color.WHITE));

        // Wait until the status indicator finishes animating, or becomes fully visible.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(getStatusIndicator().getHeight()));
                    Criteria.checkThat(
                            getStatusIndicator().getVisibility(), Matchers.is(View.VISIBLE));
                });
    }

    @Test
    @MediumTest
    public void testShowAndHideOnNtp() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        final int viewId = View.generateViewId();
        final View view = tab.getNativePage().getView();
        view.setId(viewId);

        // R.id.status_indicator won't be in the View tree until the indicator is shown for the
        // first time and the corresponding ViewStub is inflated.
        onView(withId(R.id.status_indicator)).check(doesNotExist());
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        onView(withId(viewId)).check(matches(withTopMargin(0)));
        Assert.assertFalse(
                "Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.show(
                            "Status", null, Color.BLACK, Color.WHITE, Color.WHITE);
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // Wait until the status indicator finishes animating.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(getStatusIndicator().getHeight()));
                });

        // The status indicator will be immediately visible.
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(getStatusIndicator().getHeight())));
        onView(withId(viewId)).check(matches(withTopMargin(getStatusIndicator().getHeight())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.updateContent(
                            "Exit status",
                            null,
                            Color.WHITE,
                            Color.BLACK,
                            Color.BLACK,
                            CallbackUtils.emptyRunnable());
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // #updateContent shouldn't change the layout.
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(getStatusIndicator().getHeight())));
        onView(withId(viewId)).check(matches(withTopMargin(getStatusIndicator().getHeight())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.hide();
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(0));
                });

        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        onView(withId(viewId)).check(matches(withTopMargin(0)));
    }

    @Test
    @MediumTest
    public void testShowAndHideOnRecentTabsPage() {
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        RecentTabsPageTestUtils.waitForRecentTabsPageLoaded(tab);

        // R.id.status_indicator won't be in the View tree until the indicator is shown for the
        // first time and the corresponding ViewStub is inflated.
        onView(withId(R.id.status_indicator)).check(doesNotExist());
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        onView(withId(R.id.recent_tabs_root))
                .check(
                        matches(
                                withTopMargin(
                                        mBrowserControlsStateProvider.getTopControlsHeight())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.show(
                            "Status", null, Color.BLACK, Color.WHITE, Color.WHITE);
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // Wait until the status indicator finishes animating.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(getStatusIndicator().getHeight()));
                });

        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(getStatusIndicator().getHeight())));
        onView(withId(R.id.recent_tabs_root))
                .check(
                        matches(
                                withTopMargin(
                                        mBrowserControlsStateProvider.getTopControlsHeight())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.updateContent(
                            "Exit status",
                            null,
                            Color.WHITE,
                            Color.BLACK,
                            Color.BLACK,
                            CallbackUtils.emptyRunnable());
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // #updateContent shouldn't change the layout.
        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.control_container))
                .check(matches(withTopMargin(getStatusIndicator().getHeight())));
        onView(withId(R.id.recent_tabs_root))
                .check(
                        matches(
                                withTopMargin(
                                        mBrowserControlsStateProvider.getTopControlsHeight())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStatusIndicatorCoordinator.hide();
                    mStatusIndicatorCoordinator
                            .getMediatorForTesting()
                            .finishAnimationsForTesting();
                });

        // Wait until the status indicator finishes animating, or becomes fully hidden.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mBrowserControlsStateProvider.getTopControlsMinHeightOffset(),
                            Matchers.is(0));
                });

        onView(withId(R.id.status_indicator)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.control_container)).check(matches(withTopMargin(0)));
        onView(withId(R.id.recent_tabs_root))
                .check(
                        matches(
                                withTopMargin(
                                        mBrowserControlsStateProvider.getTopControlsHeight())));
    }

    private View getStatusIndicator() {
        return mActivityTestRule.getActivity().findViewById(R.id.status_indicator);
    }

    private static Matcher<View> withTopMargin(final int expected) {
        return new TypeSafeMatcher<View>() {
            private int mActual;

            @Override
            public boolean matchesSafely(final View view) {
                mActual = getTopMarginOf(view);
                return mActual == expected;
            }

            @Override
            public void describeTo(final Description description) {
                // TODO(sinansahin): This is a work-around because the message from
                // #describeMismatchSafely is ignored. If it is fixed one day, we can implement
                // #describeMismatchSafely.
                description
                        .appendText("View should have a topMargin of " + expected)
                        .appendText(System.lineSeparator())
                        .appendText("but actually has " + mActual);
            }
        };
    }

    private static int getTopMarginOf(View view) {
        final ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        return layoutParams.topMargin;
    }
}
