// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for {@link ExploreSurfaceViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExploreSurfaceViewBinderTest {
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;
    private View mFeedSurfaceView;
    private PropertyModel mPropertyModel;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ScrollableContainerDelegate mScrollableContainerDelegate;

    @Mock
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();

        // TODO(crbug.com/1025296): Investigate to use BlankUiTestActivityTestCase. We can not do
        // that since mocked FeedSurfaceCoordinator does not work as expected in release build
        // (works well in debug build).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
            mExploreSurfaceCoordinatorFactory =
                    new ExploreSurfaceCoordinatorFactory(mActivityTestRule.getActivity(),
                            mActivityTestRule.getActivity().getCompositorViewHolderForTesting(),
                            mPropertyModel, mBottomSheetController, new ObservableSupplierImpl<>(),
                            mScrollableContainerDelegate,
                            mActivityTestRule.getActivity().getSnackbarManager(),
                            mActivityTestRule.getActivity().getShareDelegateSupplier(),
                            mActivityTestRule.getActivity().getWindowAndroid(),
                            mActivityTestRule.getActivity().getTabModelSelector(),
                            () -> { return null; }, 0L, null, null);
            mExploreSurfaceCoordinator = mExploreSurfaceCoordinatorFactory.create(
                    false, /* isPlaceholderShown= */ false, NewTabPageLaunchOrigin.UNKNOWN);
            mFeedSurfaceView = mExploreSurfaceCoordinator.getView();
        });
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithoutFeedSurfaceCoordinator() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertNull(mFeedSurfaceView.getParent());
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithFeedSurfaceCoordinator() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(IS_SHOWING_OVERVIEW, false));
        assertNull(mFeedSurfaceView.getParent());
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithBottomBarVisible() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);
            mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
            mPropertyModel.set(TOP_MARGIN, 20);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });

        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals(layoutParams.bottomMargin, 10);
        assertEquals(layoutParams.topMargin, 20);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(IS_SHOWING_OVERVIEW, false));
        assertNull(mFeedSurfaceView.getParent());
    }

    @Test
    @SmallTest
    public void testSetVisibilityAfterShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        });
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true));
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(IS_SHOWING_OVERVIEW, false));
        assertNull(mFeedSurfaceView.getParent());
    }

    @Test
    @SmallTest
    public void testSetVisibilityBeforeShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(IS_SHOWING_OVERVIEW, true));
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(IS_SHOWING_OVERVIEW, false));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false));
        assertNull(mFeedSurfaceView.getParent());
    }

    @Test
    @SmallTest
    public void testSetTopMarginWithBottomBarVisible() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);
            mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
            mPropertyModel.set(TOP_MARGIN, 20);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals("Top margin isn't initialized correctly.", 20, layoutParams.topMargin);

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(TOP_MARGIN, 40));
        layoutParams = (ViewGroup.MarginLayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals("Wrong top margin.", 40, layoutParams.topMargin);
    }

    @Test
    @SmallTest
    public void testSetTopMarginWithBottomBarNotVisible() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertNull(mFeedSurfaceView.getParent());
        assertFalse(mPropertyModel.get(IS_BOTTOM_BAR_VISIBLE));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(EXPLORE_SURFACE_COORDINATOR, mExploreSurfaceCoordinator);
            mPropertyModel.set(TOP_MARGIN, 20);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals("Wrong top margin.", 0, layoutParams.topMargin);

        TestThreadUtils.runOnUiThreadBlocking(() -> mPropertyModel.set(TOP_MARGIN, 40));

        // Top margin shouldn't add a margin if the bottom bar is not visible.
        layoutParams = (ViewGroup.MarginLayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals("Wrong top margin.", 0, layoutParams.topMargin);
    }
}
