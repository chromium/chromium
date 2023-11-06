// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData;
import android.graphics.PointF;
import android.view.View;

import androidx.core.view.ContentInfoCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link TabDropTarget}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
public class TabDropTargetTest {

    private static final int CURRENT_INSTANCE_ID = 1;
    private static final int ANOTHER_INSTANCE_ID = 2;
    // clang-format on
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private StripLayoutHelperManager mStripLayoutHelperManager;
    @Mock private StripLayoutHelper mStripLayoutHelper;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private StripLayoutTab mStripLayoutTab;
    @Mock private View mToolbarContainerView;
    @Mock private Profile mProfile;
    private TabDropTarget mTabDropTarget;
    private Activity mActivity;
    private ContentInfoCompat mContentInfoCompatPayload;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        // Create both the TabDragSource and TabDropTarget.
        mTabDropTarget =
                new TabDropTarget(
                        mStripLayoutHelperManager, mMultiInstanceManager, mToolbarContainerView);

        // Mock the tab to be dragged.
        DragDropGlobalState.getInstance().tabBeingDragged =
                MockTab.createAndInitialize(5, mProfile);
        DragDropGlobalState.getInstance().dragSourceInstanceId = CURRENT_INSTANCE_ID;

        // Mock strip methods.
        when(mStripLayoutHelperManager.getActiveStripLayoutHelper()).thenReturn(mStripLayoutHelper);

        // Create drop payload from the drag source view.
        mContentInfoCompatPayload =
                new ContentInfoCompat.Builder(
                                createClipData(
                                        mToolbarContainerView,
                                        DragDropGlobalState.getInstance().tabBeingDragged),
                                ContentInfoCompat.SOURCE_DRAG_AND_DROP)
                        .build();
    }

    @After
    public void tearDown() {
        mContentInfoCompatPayload = null;
        mToolbarContainerView = null;
        mTabDropTarget = null;
        DragDropGlobalState.getInstance().reset();
    }

    private ClipData createClipData(View view, Tab tab) {
        ClipData.Item item =
                new ClipData.Item("TabId=" + (tab != null ? tab.getId() : Tab.INVALID_TAB_ID));
        ClipData dragData =
                new ClipData(
                        (CharSequence) view.getTag(),
                        ChromeDragAndDropBrowserDelegate.SUPPORTED_MIME_TYPES,
                        item);
        return dragData;
    }

    /**
     * Tests the method {@link TabDropTarget#onReceiveContent(View, ContentInfoCompat)}. Checks that
     * it successfully accepts a drop of a payload on different tabs layout view.
     */
    @Test
    public void test_TabDropAction_onDifferentTabsLayout_ReturnsSuccess() {
        // Mock different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        DragDropGlobalState.getInstance().acceptNextDrop = true;
        DragDropGlobalState.getInstance().dropLocation = new PointF(100f, 20.f);
        Mockito.doNothing()
                .when(mMultiInstanceManager)
                .moveTabToWindow(
                        any(), eq(DragDropGlobalState.getInstance().tabBeingDragged), anyInt());

        // Perform action of a simulated drop of ClipData payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, mContentInfoCompatPayload);

        // Verify the ClipData dropped was consumed.
        assertNull("Remaining payload should be null.", remainingPayload);
    }

    /**
     * Tests the method {@link TabDropTarget#onReceiveContent(View, ContentInfoCompat)}. Checks that
     * it successfully rejects the drop of payload on same tab layout view.
     */
    @Test
    public void test_TabDropAction_onSameTabsLayout_ReturnsFalse() {
        // Mock same tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURRENT_INSTANCE_ID);

        DragDropGlobalState.getInstance().acceptNextDrop = true;

        // Perform action of a simulated drop of ClipData payload on drag source view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, mContentInfoCompatPayload);

        // Verify the drop was not consumed, the original payload was returned.
        assertEquals(
                "Original payload should be returned.",
                mContentInfoCompatPayload,
                remainingPayload);
    }

    @Test
    public void test_TabDropAction_doNotAcceptNextDrop_ReturnsFalse() {
        // Mock different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);
        DragDropGlobalState.getInstance().acceptNextDrop = false;

        // Perform action of a simulated drop of ClipData payload on drag source view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, mContentInfoCompatPayload);

        // Verify the drop was not consumed, the original payload was returned.
        assertEquals(
                "Original payload should be returned.",
                mContentInfoCompatPayload,
                remainingPayload);
    }

    /**
     * Tests the method {@link TabDropTarget#onReceiveContent(View, ContentInfoCompat)}. Checks that
     * it rejects a ClipData drop of a payload with the tabId that does not match with the saved Tab
     * info even if the source and destination windows are different.
     */
    @Test
    public void test_TabDropAction_onDifferentTabsLayout_WithBadClipDataDrop_ReturnsFailure() {
        // Mock different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        DragDropGlobalState.getInstance().acceptNextDrop = true;
        DragDropGlobalState.getInstance().dropLocation = new PointF(100f, 20.f);

        // Create ClipData for non-Chrome apps with invalid tab id.
        Tab tabForBadClipData = MockTab.createAndInitialize(555, mProfile);
        ContentInfoCompat compactPayloadWithBadClipData =
                new ContentInfoCompat.Builder(
                                createClipData(mToolbarContainerView, tabForBadClipData),
                                ContentInfoCompat.SOURCE_DRAG_AND_DROP)
                        .build();

        // Perform action of a simulated drop of ClipData payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(
                        mToolbarContainerView, compactPayloadWithBadClipData);

        // Verify the ClipData dropped was rejected.
        assertEquals(
                "Original payload should be returned.",
                compactPayloadWithBadClipData,
                remainingPayload);
    }

    @Test
    public void test_TabDropAction_onDifferentTabsLayout_WithNullClipDataDrop_ReturnsFailure() {
        // Mock different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        DragDropGlobalState.getInstance().acceptNextDrop = true;

        // Perform action of a simulated drop of null payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, null);

        // Verify the ClipData dropped was rejected.
        assertNull("Dropped null payload should be returned.", remainingPayload);
    }

    @Test
    public void test_TabDropAction_onDifferentTabsLayout_WithEmptyClipDataDrop_ReturnsFailure() {
        // Mock different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        DragDropGlobalState.getInstance().acceptNextDrop = true;

        // Create empty ClipData.
        ContentInfoCompat compactPayloadWithEmptyClipData =
                new ContentInfoCompat.Builder(
                                createClipData(mToolbarContainerView, null),
                                ContentInfoCompat.SOURCE_DRAG_AND_DROP)
                        .build();

        // Perform action of a simulated drop of empty ClipData payload on drop target view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(
                        mToolbarContainerView, compactPayloadWithEmptyClipData);

        // Verify the ClipData dropped was rejected.
        assertEquals(
                "Dropped same payload should be returned.",
                compactPayloadWithEmptyClipData,
                remainingPayload);
    }
    @Test
    public void test_TabDropAction_onDifferentTabsLayout_onTab_inRTLLayout_success() {
        LocalizationUtils.setRtlForTesting(true);
        // Simulate different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        // Drop point is on the left side of the tab.
        final int droppedOnIndex = 1;
        prepareDropOnTab(3, droppedOnIndex, 1, new PointF(140.f, 20.f), 101.f, 100.f);

        // Perform action of a simulated drop of ClipData payload on a desired tab of drop target
        // view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, mContentInfoCompatPayload);

        // Verify the ClipData dropped was consumed and the tab is positioned on a desired location.
        assertNull("Dropped payload should be fully consumed.", remainingPayload);
        verify(mStripLayoutHelper, times(1)).selectTabAtIndex(droppedOnIndex + 1);
    }

    @Test
    public void test_TabDropAction_onDifferentTabsLayout_onTab_inLTRLayout_success() {
        LocalizationUtils.setRtlForTesting(false);
        // Simulate different tab layout.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);

        // Drop point is on the right side of the tab.
        final int droppedOnIndex = 1;
        prepareDropOnTab(3, droppedOnIndex, 1, new PointF(160.f, 20.f), 101.f, 100.f);

        // Perform action of a simulated drop of ClipData payload on a desired tab of drop target
        // view.
        ContentInfoCompat remainingPayload =
                mTabDropTarget.onReceiveContent(mToolbarContainerView, mContentInfoCompatPayload);

        // Verify the ClipData dropped was consumed and the inset tab is desired location.
        assertNull("Dropped payload should be fully consumed.", remainingPayload);
        verify(mStripLayoutHelper, times(1)).selectTabAtIndex(droppedOnIndex + 1);
    }

    private void prepareDropOnTab(
            int tabsCount,
            int droppedOnIndex,
            int droppedOnTabId,
            PointF dropPoint,
            float tabStartX,
            float tabWidth) {
        DragDropGlobalState.getInstance().acceptNextDrop = true;
        DragDropGlobalState.getInstance().dropLocation = dropPoint;
        when(mStripLayoutHelper.getTabAtPosition(anyFloat())).thenReturn(mStripLayoutTab);
        when(mStripLayoutHelper.getTabCount()).thenReturn(tabsCount);
        when(mStripLayoutHelper.findIndexForTab(droppedOnTabId)).thenReturn(droppedOnIndex);
        when(mStripLayoutTab.getId()).thenReturn(droppedOnTabId);
        when(mStripLayoutTab.getDrawX()).thenReturn(tabStartX);
        when(mStripLayoutTab.getWidth()).thenReturn(tabWidth);
    }
}
