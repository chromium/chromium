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
import android.widget.LinearLayout;

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
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyKey;
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
    private LinearLayout mSupplementaryContainer;
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
                    mSupplementaryContainer = new LinearLayout(sActivity);
                    mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mContainerModel,
                                    new TabListContainerViewBinder.ViewHolder(
                                            mRecyclerView, mHairline, mSupplementaryContainer),
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // crbug.com/376527109
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
        SettableNonNullObservableSupplier<Boolean> isAnimatingSupplier =
                ObservableSuppliers.createNonNull(false);
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
        SettableNonNullObservableSupplier<Boolean> isAnimatingSupplier =
                ObservableSuppliers.createNonNull(false);

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
                        new TabListContainerViewBinder.ViewHolder(
                                mRecyclerView, mHairline, mSupplementaryContainer),
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
    public void testAnimateSupplementaryContainer() {
        // Mock the necessary suppliers.
        SettableNonNullObservableSupplier<Boolean> manualAnimationSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Boolean> hubVisibilitySupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNonNullObservableSupplier<Float> fractionSupplier =
                ObservableSuppliers.createNonNull(0f);

        mContainerModel.set(
                TabListContainerProperties.MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER,
                manualAnimationSupplier);
        mContainerModel.set(
                TabListContainerProperties.HUB_SEARCH_BOX_VISIBILITY_SUPPLIER,
                hubVisibilitySupplier);
        mContainerModel.set(
                TabListContainerProperties.SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER,
                fractionSupplier);

        // Simulate animation to show search box.
        TabListContainerProperties.SupplementaryContainerAnimationMetadata metadata =
                new TabListContainerProperties.SupplementaryContainerAnimationMetadata(true, false);
        mContainerModel.set(TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER, metadata);

        // Verify initial animation state.
        assertTrue(manualAnimationSupplier.get());
        assertTrue(hubVisibilitySupplier.get());
        assertEquals(0f, fractionSupplier.get(), 0.001f);

        // Simulate animation end.
        TabListContainerViewBinder.sSupplementaryContainerAnimationHandler.forceFinishAnimation();
        assertFalse(manualAnimationSupplier.get());
        assertTrue(hubVisibilitySupplier.get());
        assertEquals(1f, fractionSupplier.get(), 0.001f);

        // Simulate animation to hide search box.
        metadata =
                new TabListContainerProperties.SupplementaryContainerAnimationMetadata(
                        false, false);
        mContainerModel.set(TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER, metadata);

        // Verify initial animation state for hiding.
        assertTrue(manualAnimationSupplier.get());
        assertTrue(hubVisibilitySupplier.get());
        assertEquals(1f, fractionSupplier.get(), 0.001f);

        // Simulate animation end.
        TabListContainerViewBinder.sSupplementaryContainerAnimationHandler.forceFinishAnimation();
        assertFalse(manualAnimationSupplier.get());
        assertFalse(hubVisibilitySupplier.get());
        assertEquals(0f, fractionSupplier.get(), 0.001f);

        // Test forced update when already at target.
        metadata =
                new TabListContainerProperties.SupplementaryContainerAnimationMetadata(true, false);
        float targetTranslationY =
                mRecyclerView.getResources().getDimensionPixelSize(R.dimen.hub_search_box_gap);
        mSupplementaryContainer.setTranslationY(targetTranslationY);
        mContainerModel.set(TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER, metadata);

        // No animation should start if not forced and already at target.
        assertFalse(
                TabListContainerViewBinder.sSupplementaryContainerAnimationHandler
                        .isAnimationPresent());

        metadata =
                new TabListContainerProperties.SupplementaryContainerAnimationMetadata(
                        true, true); // Forced to true
        mContainerModel.set(TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER, metadata);

        // Animation should start because it's forced.
        assertTrue(
                TabListContainerViewBinder.sSupplementaryContainerAnimationHandler
                        .isAnimationPresent());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testAnimateSupplementaryContainer_NoContainer() {
        PropertyModel containerModelWithoutSupplementaryContainer =
                new PropertyModel(TabListContainerProperties.ALL_KEYS);
        PropertyModelChangeProcessor<
                        PropertyModel, TabListContainerViewBinder.ViewHolder, PropertyKey>
                cpWithoutSupplementaryContainer =
                        PropertyModelChangeProcessor.create(
                                containerModelWithoutSupplementaryContainer,
                                new TabListContainerViewBinder.ViewHolder(
                                        mRecyclerView, mHairline, null),
                                TabListContainerViewBinder::bind);

        // Ensure no crash when supplementaryDataContainer is null.
        TabListContainerProperties.SupplementaryContainerAnimationMetadata metadata =
                new TabListContainerProperties.SupplementaryContainerAnimationMetadata(true, false);
        containerModelWithoutSupplementaryContainer.set(
                TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER, metadata);
        cpWithoutSupplementaryContainer.destroy();
    }
}
