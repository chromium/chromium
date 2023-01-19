// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.MathUtils;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
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
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
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
    private CallbackHelper mStartedShowingCallback;
    private CallbackHelper mFinishedShowingCallback;
    private CallbackHelper mStartedHidingCallback;
    private CallbackHelper mFinishedHidingCallback;
    private boolean mIsAnimating;

    private TabListRecyclerView.VisibilityListener mMockVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    mStartedShowingCallback.notifyCalled();
                    mIsAnimating = isAnimating;
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
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisableIf.Build(hardware_is = "bullhead", message = "Flaky on CFI bot. " +
            "https://crbug.com/954145")
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "https://crbug.com/1182554")
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
    @Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
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
    public void testIsIncognitoSetsBackgroundColor() {
        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, true);
        assertThat(mRecyclerView.getBackground(), instanceOf(ColorDrawable.class));
        assertThat(((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                equalTo(mRecyclerView.getContext().getColor(R.color.default_bg_color_dark)));

        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, false);
        assertThat(mRecyclerView.getBackground(), instanceOf(ColorDrawable.class));
        assertThat(((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                equalTo(SemanticColorUtils.getDefaultBgColor(mRecyclerView.getContext())));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTopMarginSetsTopMargin() {
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(
                ((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin, equalTo(0));

        mContainerModel.set(TabListContainerProperties.TOP_MARGIN, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin,
                equalTo(CONTAINER_HEIGHT));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testBottomContainerHeightSetsBottomMargin() {
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(0));

        mContainerModel.set(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(CONTAINER_HEIGHT));
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

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }
}
