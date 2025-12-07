// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link TabListRecyclerView} and {@link TabListContainerViewBinder} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabListContainerViewBinderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private PropertyModel mContainerModel;
    private PropertyModelChangeProcessor mMCP;
    private TabListRecyclerView mRecyclerView;
    private FrameLayout mContentView;
    private ImageView mHairline;
    @Spy private GridLayoutManager mGridLayoutManager;
    @Spy private LinearLayoutManager mLinearLayoutManager;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.tab_switcher_pane_layout);
                    mContentView = sActivity.findViewById(android.R.id.content);
                    mRecyclerView =
                            (TabListRecyclerView)
                                    sActivity
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_list_recycler_view_layout, null);
                    ((FrameLayout) mContentView.findViewById(R.id.tab_list_container))
                            .addView(mRecyclerView);
                    mHairline = mContentView.findViewById(R.id.pane_hairline);
                    mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mContainerModel,
                                    new TabListContainerViewBinder.ViewHolder(
                                            mRecyclerView, mHairline),
                                    TabListContainerViewBinder::bind);
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
    }

    private void setUpGridLayoutManager() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGridLayoutManager = spy(new GridLayoutManager(sActivity, 2));
                    mRecyclerView.setLayoutManager(mGridLayoutManager);
                });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetBottomPadding() {
        int oldLeft = mRecyclerView.getPaddingLeft();
        int oldTop = mRecyclerView.getPaddingTop();
        int oldRight = mRecyclerView.getPaddingRight();
        int oldBottom = mRecyclerView.getPaddingBottom();

        int customBottom = 37;
        mContainerModel.set(TabListContainerProperties.BOTTOM_PADDING, customBottom);

        int left = mRecyclerView.getPaddingLeft();
        int top = mRecyclerView.getPaddingTop();
        int right = mRecyclerView.getPaddingRight();
        int bottom = mRecyclerView.getPaddingBottom();

        assertEquals(oldLeft, left);
        assertEquals(oldTop, top);
        assertEquals(oldRight, right);
        assertNotEquals(oldBottom, customBottom);
        assertEquals(bottom, customBottom);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetClipToPadding() {
        mContainerModel.set(TabListContainerProperties.IS_CLIP_TO_PADDING, false);
        assertFalse(mRecyclerView.getClipToPadding());

        mContainerModel.set(TabListContainerProperties.IS_CLIP_TO_PADDING, true);
        assertTrue(mRecyclerView.getClipToPadding());
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
                .scrollToPositionWithOffset(
                        eq(5),
                        intThat(allOf(lessThan(mRecyclerView.getHeight() / 2), greaterThan(0))));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testSetIsContentSensitive() {
        // Chances are the sensitivity is set to auto initially. That's not a problem, it just needs
        // not to be sensitive.
        assertNotEquals(View.CONTENT_SENSITIVITY_SENSITIVE, mRecyclerView.getContentSensitivity());
        mContainerModel.set(TabListContainerProperties.IS_CONTENT_SENSITIVE, true);
        assertEquals(View.CONTENT_SENSITIVITY_SENSITIVE, mRecyclerView.getContentSensitivity());
        mContainerModel.set(TabListContainerProperties.IS_CONTENT_SENSITIVE, false);
        assertEquals(View.CONTENT_SENSITIVITY_NOT_SENSITIVE, mRecyclerView.getContentSensitivity());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHairlineVisibility() {
        ObservableSupplierImpl<Boolean> isAnimatingSupplier = new ObservableSupplierImpl<>(false);
        // The hairline is hidden when the pinned tab strip is animating.
        mContainerModel.set(
                TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER,
                isAnimatingSupplier);
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, true);
        assertEquals(View.VISIBLE, mHairline.getVisibility());

        // The hairline is not visible when IS_HAIRLINE_VISIBLE is false.
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, false);
        assertEquals(View.GONE, mHairline.getVisibility());

        // When the animation starts, the hairline becomes invisible.
        isAnimatingSupplier.set(true);

        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, true);
        assertEquals(View.GONE, mHairline.getVisibility());

        isAnimatingSupplier.set(false);
        assertEquals(View.VISIBLE, mHairline.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHairlineVisibility_InitialState() {
        ObservableSupplierImpl<Boolean> isAnimatingSupplier = new ObservableSupplierImpl<>(false);

        // Initial state: not visible, not animating
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, false);
        mContainerModel.set(
                TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER,
                isAnimatingSupplier);
        assertEquals(View.GONE, mHairline.getVisibility());

        // Initial state: visible, not animating
        mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);
        mMCP =
                PropertyModelChangeProcessor.create(
                        mContainerModel,
                        new TabListContainerViewBinder.ViewHolder(mRecyclerView, mHairline),
                        TabListContainerViewBinder::bind);
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, true);
        mContainerModel.set(
                TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER,
                isAnimatingSupplier);
        assertEquals(View.VISIBLE, mHairline.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHairlineVisibility_NullAnimationSupplier() {
        // Set supplier to null
        mContainerModel.set(
                TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER, null);

        // Hairline should be visible if IS_HAIRLINE_VISIBLE is true, even with null supplier
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, true);
        assertEquals(View.VISIBLE, mHairline.getVisibility());

        // Hairline should be gone if IS_HAIRLINE_VISIBLE is false, even with null supplier
        mContainerModel.set(TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, false);
        assertEquals(View.GONE, mHairline.getVisibility());
    }
}
