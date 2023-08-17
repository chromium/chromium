// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.CommandLine;
import org.chromium.base.MathUtils;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests for {@link TabListRecyclerView} and {@link TabListContainerViewBinder}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabListContainerViewBinderTest extends BlankUiTestActivityTestCase {
    /**
     * BlankUiTestActivityTestCase also needs {@link ChromeFeatureList}'s
     * internal test-only feature map, not the {@link CommandLine} provided by
     * {@link Features.InstrumentationProcessor}.
     */
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int CONTAINER_HEIGHT = 56;
    private static final int INCREASED_CONTAINER_HEIGHT = 76;
    private PropertyModel mContainerModel;
    private PropertyModelChangeProcessor mMCP;
    private TabListRecyclerView mRecyclerView;
    @Spy
    private GridLayoutManager mGridLayoutManager;
    @Spy
    private LinearLayoutManager mLinearLayoutManager;
    private CallbackHelper mStartedShowingCallback;
    private CallbackHelper mFinishedShowingCallback;
    private CallbackHelper mStartedHidingCallback;
    private CallbackHelper mFinishedHidingCallback;
    private boolean mIsAnimating;
    private boolean mShouldShowShadow;

    private TabListRecyclerView.VisibilityListener mMockVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    mStartedShowingCallback.notifyCalled();
                    mIsAnimating = isAnimating;
                    // Simulate invocation of #setShadowVisibility to reflect actual method call.
                    mRecyclerView.setShadowVisibility(mShouldShowShadow);
                }

                @Override
                public void finishedShowing() {
                    mFinishedShowingCallback.notifyCalled();
                }

                @Override
                public void startedHiding(boolean isAnimating) {
                    mStartedHidingCallback.notifyCalled();
                    mIsAnimating = isAnimating;
                }

                @Override
                public void finishedHiding() {
                    mFinishedHidingCallback.notifyCalled();
                }
            };

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        BlankUiTestActivity.setTestLayout(R.layout.tab_list_recycler_view_layout);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRecyclerView = getActivity().findViewById(R.id.tab_list_view); });

        mStartedShowingCallback = new CallbackHelper();
        mFinishedShowingCallback = new CallbackHelper();
        mStartedHidingCallback = new CallbackHelper();
        mFinishedHidingCallback = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

            mMCP = PropertyModelChangeProcessor.create(
                    mContainerModel, mRecyclerView, TabListContainerViewBinder::bind);
        });

        mShouldShowShadow = false;
    }

    private void setUpGridLayoutManager() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridLayoutManager = spy(new GridLayoutManager(getActivity(), 2));
            mRecyclerView.setLayoutManager(mGridLayoutManager);
        });
    }

    private void setUpLinearLayoutManager() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLinearLayoutManager = spy(new LinearLayoutManager(getActivity()));
            mRecyclerView.setLayoutManager(mLinearLayoutManager);
        });
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testShowWithAnimation() {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, true);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        });
        assertThat(mStartedShowingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        if (areAnimatorsEnabled()) {
            assertThat(mRecyclerView.getAlpha(), equalTo(0.0f));
        }
        assertThat(mIsAnimating, equalTo(true));

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mRecyclerView.getAlpha(), Matchers.is(1.0f)));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testShowWithoutAnimation() {
        mContainerModel.set(
                TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

        mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);

        assertThat(mStartedShowingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.isAnimating(), equalTo(false));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));
        assertThat(mFinishedShowingCallback.getCallCount(), equalTo(1));
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testShowWithAnimation_showShadow() {
        // clang-format on
        mShouldShowShadow = true;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, false);
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);
            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, true);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        });

        ImageView shadowImage = mRecyclerView.getShadowImageViewForTesting();
        int toolbarHairlineColor = ThemeUtils.getToolbarHairlineColor(mRecyclerView.getContext(),
                ChromeColors.getPrimaryBackgroundColor(mRecyclerView.getContext(), false), false);
        assertEquals("Toolbar hairline color for the regular tab model should match.",
                ColorStateList.valueOf(toolbarHairlineColor), shadowImage.getImageTintList());

        // Switch to incognito, shadow image color should update.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, true));
        toolbarHairlineColor = ThemeUtils.getToolbarHairlineColor(mRecyclerView.getContext(),
                ChromeColors.getPrimaryBackgroundColor(mRecyclerView.getContext(), true), true);
        assertEquals("Toolbar hairline color for the incognito tab model should match.",
                ColorStateList.valueOf(toolbarHairlineColor), shadowImage.getImageTintList());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testHidesWithAnimation() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        });

        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, true);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);
        });

        assertThat(mStartedHidingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        if (areAnimatorsEnabled()) {
            assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));
        }
        assertThat(mIsAnimating, equalTo(true));
        // Invisibility signals the end of the animation, not alpha being zero.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mRecyclerView.getVisibility(), Matchers.is(View.INVISIBLE));
        });
        assertThat(mRecyclerView.getAlpha(), equalTo(0.0f));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testHidesWithoutAnimation() {
        mContainerModel.set(
                TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

        mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);

        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);

        assertThat(mStartedHidingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.isAnimating(), equalTo(false));
        assertThat(mRecyclerView.getAlpha(), equalTo(0.0f));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.INVISIBLE));
        assertThat(mFinishedHidingCallback.getCallCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testIsIncognitoSetsBackgroundAndToolbarHairlineColor() {
        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, true);
        assertTrue("View background should be an instance of ColorDrawable.",
                mRecyclerView.getBackground() instanceof ColorDrawable);
        assertEquals("View background color for the incognito tab model should match.",
                ((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                mRecyclerView.getContext().getColor(R.color.default_bg_color_dark));
        assertEquals(
                "View toolbar hairline drawable color for the incognito tab model should match.",
                ThemeUtils.getToolbarHairlineColor(mRecyclerView.getContext(),
                        ChromeColors.getPrimaryBackgroundColor(mRecyclerView.getContext(), true),
                        true),
                mRecyclerView.getToolbarHairlineColorForTesting());

        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, false);
        assertTrue("View background should be an instance of ColorDrawable.",
                mRecyclerView.getBackground() instanceof ColorDrawable);
        assertEquals("View background color for the regular tab model should match.",
                ((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                SemanticColorUtils.getDefaultBgColor(mRecyclerView.getContext()));
        assertEquals("View toolbar hairline drawable color for the regular tab model should match.",
                ThemeUtils.getToolbarHairlineColor(mRecyclerView.getContext(),
                        ChromeColors.getPrimaryBackgroundColor(mRecyclerView.getContext(), false),
                        false),
                mRecyclerView.getToolbarHairlineColorForTesting());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTopMarginSetsTopMargin() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);
            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        });
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(
                ((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin, equalTo(0));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);
        mContainerModel.set(TabListContainerProperties.TOP_MARGIN, CONTAINER_HEIGHT);
        assertThat(
                ((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin, equalTo(0));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin,
                equalTo(CONTAINER_HEIGHT));

        mContainerModel.set(TabListContainerProperties.TOP_MARGIN, CONTAINER_HEIGHT + 1);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin,
                equalTo(CONTAINER_HEIGHT + 1));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);
        mContainerModel.set(TabListContainerProperties.TOP_MARGIN, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin,
                equalTo(CONTAINER_HEIGHT + 1));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testBottomContainerHeightSetsBottomMargin() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);
            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        });
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(0));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);
        mContainerModel.set(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(0));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(CONTAINER_HEIGHT));

        mContainerModel.set(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT, 0);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(0));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetShadowTopOffsetUpdatesTranslation() {
        mContainerModel.set(
                TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

        mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);

        ImageView shadowImageView = mRecyclerView.getShadowImageViewForTesting();

        assertEquals(0, shadowImageView.getTranslationY(), MathUtils.EPSILON);

        mContainerModel.set(
                TabListContainerProperties.SHADOW_TOP_OFFSET, INCREASED_CONTAINER_HEIGHT);
        assertEquals(
                INCREASED_CONTAINER_HEIGHT, shadowImageView.getTranslationY(), MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testBottomPaddingSetsBottomPadding() {
        assertThat(mRecyclerView.getPaddingBottom(), equalTo(0));

        mContainerModel.set(TabListContainerProperties.BOTTOM_PADDING, CONTAINER_HEIGHT);
        assertThat(mRecyclerView.getPaddingBottom(), equalTo(CONTAINER_HEIGHT));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_Carousel() {
        setUpLinearLayoutManager();
        mRecyclerView.layout(0, 0, 1000, 100);

        mContainerModel.set(
                TabListContainerProperties.MODE, TabListCoordinator.TabListMode.CAROUSEL);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 3);

        // Offset will be view width (1000) / 2 - tab card width calculated from dp dimension / 2.
        verify(mLinearLayoutManager, times(1))
                .scrollToPositionWithOffset(eq(3),
                        intThat(allOf(lessThan(mRecyclerView.getWidth() / 2), greaterThan(0))));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_Grid() {
        setUpGridLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.GRID);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 5);

        // Offset will be view height (500) / 2 - tab card height calculated from TabUtils / 2
        verify(mGridLayoutManager, times(1))
                .scrollToPositionWithOffset(eq(5),
                        intThat(allOf(lessThan(mRecyclerView.getHeight() / 2), greaterThan(0))));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_List_NoTabs() {
        setUpLinearLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.LIST);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 7);

        // Offset will be 0 to avoid divide by 0 with no tabs.
        verify(mLinearLayoutManager, times(1)).scrollToPositionWithOffset(eq(7), eq(0));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_List_WithTabs() {
        setUpLinearLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        doReturn(9).when(mLinearLayoutManager).getItemCount();
        int range = mRecyclerView.computeVerticalScrollRange();

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.LIST);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 5);

        // 9 Tabs at 900 scroll extent = 100 per tab. With view height of 500 the offset is
        // 500 / 2 - range / 9 / 2 = result.
        verify(mLinearLayoutManager, times(1))
                .scrollToPositionWithOffset(eq(5), eq(250 - range / 9 / 2));
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }
}
