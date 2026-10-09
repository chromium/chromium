// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link EnhancedTargetDevicePickerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EnhancedTargetDevicePickerViewTest {
    private static final int CONTAINER_HEIGHT_PX = 2000;
    private static final int MAX_SHEET_HEIGHT_PX = 1600;
    private static final int MAX_SHEET_WIDTH_PX = 450;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mBottomSheetController.getMaxSheetWidth()).thenReturn(MAX_SHEET_WIDTH_PX);
        when(mBottomSheetController.requestShowContent(any(), any(Boolean.class))).thenReturn(true);
    }

    private EnhancedTargetDevicePickerView createViewWithDevices(
            int deviceCount, boolean isLargeFormFactorUiEnabled) {
        when(mBottomSheetController.isLargeFormFactorUiEnabled(any()))
                .thenReturn(isLargeFormFactorUiEnabled);
        when(mBottomSheetController.getMaxSheetHeight()).thenReturn(MAX_SHEET_HEIGHT_PX);
        when(mBottomSheetController.getContainerHeight()).thenReturn(CONTAINER_HEIGHT_PX);

        EnhancedTargetDevicePickerView view =
                new EnhancedTargetDevicePickerView(mActivity, mBottomSheetController);

        PropertyModel model = EnhancedTargetDevicePickerProperties.createDefaultModel();
        model.set(EnhancedTargetDevicePickerProperties.DISMISS_CALLBACK, reason -> {});
        new EnhancedTargetDevicePickerMediator(
                "https://example.com",
                "Example Title",
                createTargetDevices(deviceCount),
                mProfile,
                SupplierUtils.ofNull(),
                model,
                ShareEntryPoint.SHARE_SHEET);
        PropertyModelChangeProcessor.create(
                model, view, EnhancedTargetDevicePickerViewBinder::bind);
        model.set(EnhancedTargetDevicePickerProperties.VISIBLE, true);
        return view;
    }

    private static List<TargetDeviceInfo> createTargetDevices(int deviceCount) {
        List<TargetDeviceInfo> devices = new ArrayList<>();
        for (int i = 1; i <= deviceCount; i++) {
            devices.add(
                    new TargetDeviceInfo(
                            "Device " + i,
                            "guid_" + i,
                            FormFactor.PHONE,
                            OsType.ANDROID,
                            "Active today"));
        }
        return devices;
    }

    /**
     * Tests that when the sheet enters `SheetState.SCROLLING` while settling toward
     * `SheetState.HALF` on phones, `mManageDevicesBlock` remains hidden and `sheet_item_list` is
     * immediately clamped to the half-state overflow height rather than snapping after the
     * animation finishes.
     */
    @Test
    public void testOnSheetStateChanged_scrollingToHalfOnPhone_clampsListAndHidesManageDevices() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(
                        /* deviceCount= */ 5, /* isLargeFormFactorUiEnabled= */ false);
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.HALF);

        view.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);

        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());
        RecyclerView listView = view.getSheetItemListView();
        assertTrue(listView.isVerticalFadingEdgeEnabled());
        int scrollingClampedHeight = listView.getLayoutParams().height;
        assertTrue(scrollingClampedHeight > 0);

        // Once the animation settles at SheetState.HALF, the clamped list height must not snap.
        view.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());
        assertEquals(scrollingClampedHeight, listView.getLayoutParams().height);
    }

    /**
     * Tests that when the sheet enters `SheetState.SCROLLING` while settling toward
     * `SheetState.HALF` on desktop (`isLargeFormFactorUiEnabled` is `true`), `mManageDevicesBlock`
     * stays visible and `sheet_item_list` is immediately clamped so `mManageDevicesBlock` is not
     * crushed during the opening animation.
     */
    @Test
    public void testOnSheetStateChanged_scrollingToHalfOnDesktop_clampsListAndShowsManageDevices() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(/* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ true);
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.HALF);

        view.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);

        assertEquals(View.VISIBLE, view.mManageDevicesBlock.getVisibility());
        RecyclerView listView = view.getSheetItemListView();
        assertTrue(listView.isVerticalFadingEdgeEnabled());
        assertTrue(listView.getLayoutParams().height > 0);
    }

    /**
     * Tests that when the sheet enters `SheetState.SCROLLING` while settling toward
     * `SheetState.FULL`, `mManageDevicesBlock` becomes visible and the 4-device list expands to
     * `WRAP_CONTENT` without fading edges.
     */
    @Test
    public void testOnSheetStateChanged_scrollingToFull_expandsListAndShowsManageDevices() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(
                        /* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ false);
        view.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());

        // Start settling animation from HALF to FULL.
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.FULL);
        view.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.SWIPE);

        assertEquals(View.VISIBLE, view.mManageDevicesBlock.getVisibility());
        RecyclerView listView = view.getSheetItemListView();
        assertFalse(listView.isVerticalFadingEdgeEnabled());
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, listView.getLayoutParams().height);
    }

    /**
     * Tests that an active user drag (`SheetState.SCROLLING` with `targetState == SheetState.NONE`)
     * resolves to `SheetState.FULL` so the sheet reveals `mManageDevicesBlock` and unclamps the
     * list while dragging upward.
     */
    @Test
    public void testOnSheetStateChanged_scrollingDuringUserDrag_treatsStateAsFull() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(
                        /* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ false);
        view.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        // Active user drag sets current state to SCROLLING and target state to NONE.
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.NONE);
        view.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.SWIPE);

        assertEquals(View.VISIBLE, view.mManageDevicesBlock.getVisibility());
        assertEquals(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                view.getSheetItemListView().getLayoutParams().height);
    }

    /**
     * Tests that closing the sheet from `SheetState.HALF` (`SheetState.SCROLLING` with `targetState
     * == SheetState.HIDDEN`) does not flash `mManageDevicesBlock` visible or unclamp the list
     * during the closing animation.
     */
    @Test
    public void testOnSheetStateChanged_scrollingToHiddenFromHalf_preservesHalfStateLayout() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(
                        /* deviceCount= */ 5, /* isLargeFormFactorUiEnabled= */ false);
        view.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        int halfStateListHeight = view.getSheetItemListView().getLayoutParams().height;
        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());
        assertTrue(halfStateListHeight > 0);

        // Animate closed from HALF to HIDDEN.
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.HIDDEN);
        view.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.SWIPE);

        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());
        assertEquals(halfStateListHeight, view.getSheetItemListView().getLayoutParams().height);
    }

    /**
     * Tests that `getDesiredSheetHeightPx` and `getMaximumSheetHeightPx` re-measure
     * `mManageDevicesBlock` with `MeasureSpec.UNSPECIFIED` even when a prior constrained layout
     * pass crushed `mManageDevicesBlock` to a non-zero `AT_MOST` height on desktop.
     */
    @Test
    public void testGetDesiredSheetHeightPx_recoversFromClippedManageDevicesMeasurement() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(/* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ true);

        int expectedDesiredHeight = view.getDesiredSheetHeightPx();
        int expectedMaxHeight = view.getMaximumSheetHeightPx();
        int fullManageDevicesHeight = view.mManageDevicesBlock.getMeasuredHeight();
        assertTrue(fullManageDevicesHeight > 20);

        // Simulate a constrained parent layout pass that crushes mManageDevicesBlock to a non-zero
        // height of 15px (MeasureSpec.AT_MOST).
        int constrainedWidthSpec =
                View.MeasureSpec.makeMeasureSpec(MAX_SHEET_WIDTH_PX, View.MeasureSpec.AT_MOST);
        int constrainedHeightSpec = View.MeasureSpec.makeMeasureSpec(15, View.MeasureSpec.AT_MOST);
        view.mManageDevicesBlock.measure(constrainedWidthSpec, constrainedHeightSpec);
        assertEquals(15, view.mManageDevicesBlock.getMeasuredHeight());

        // Subsequent height calculations must re-measure with UNSPECIFIED and recover the full
        // unconstrained height rather than using the clipped 15px measurement.
        assertEquals(expectedDesiredHeight, view.getDesiredSheetHeightPx());
        assertEquals(expectedMaxHeight, view.getMaximumSheetHeightPx());
        assertEquals(fullManageDevicesHeight, view.mManageDevicesBlock.getMeasuredHeight());
    }

    /**
     * Tests that `onContainerSizeChanged` resolves `SheetState.SCROLLING` via `getTargetSheetState`
     * so resizing during an opening animation to `SheetState.HALF` preserves half-state visibility
     * and clamping.
     */
    @Test
    public void testOnContainerSizeChanged_scrollingToHalf_usesTargetHalfState() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(
                        /* deviceCount= */ 5, /* isLargeFormFactorUiEnabled= */ false);
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.SCROLLING);
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.HALF);

        view.onContainerSizeChanged(/* width= */ 1080, /* height= */ 1800);

        assertEquals(View.GONE, view.mManageDevicesBlock.getVisibility());
        assertTrue(view.getSheetItemListView().isVerticalFadingEdgeEnabled());
        assertTrue(view.getSheetItemListView().getLayoutParams().height > 0);
    }

    /**
     * Tests that the content view layout change listener on desktop resolves `SheetState.SCROLLING`
     * via `getTargetSheetState` and clamps `sheet_item_list` when settling toward
     * `SheetState.HALF`.
     */
    @Test
    public void testOnLayoutChange_scrollingToHalfOnDesktop_clampsListForTargetHalfState() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(/* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ true);
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.SCROLLING);
        when(mBottomSheetController.getTargetSheetState()).thenReturn(SheetState.HALF);

        // Trigger a layout pass with a height change on the content view.
        view.getContentView().layout(0, 0, MAX_SHEET_WIDTH_PX, 658);

        RecyclerView listView = view.getSheetItemListView();
        assertTrue(listView.isVerticalFadingEdgeEnabled());
        assertTrue(listView.getLayoutParams().height > 0);
    }

    /**
     * Tests that when `mSendButton` and `mManageDevicesBlock` have already been laid out
     * (`getWidth() > 0`), re-measuring their heights via `getDesiredSheetHeightPx` and
     * `getMaximumSheetHeightPx` uses `MeasureSpec.EXACTLY` with `getWidth()` so their
     * `getMeasuredWidth()` is not mutated to `AT_MOST` display or narrow text width.
     */
    @Test
    public void testGetDesiredSheetHeightPx_laidOutView_preservesExactWidth() {
        EnhancedTargetDevicePickerView view =
                createViewWithDevices(/* deviceCount= */ 4, /* isLargeFormFactorUiEnabled= */ true);

        int laidOutWidthPx = 418;
        view.mSendButton.layout(0, 0, laidOutWidthPx, 48);
        view.mManageDevicesBlock.layout(0, 48, laidOutWidthPx, 120);

        view.getDesiredSheetHeightPx();
        view.getMaximumSheetHeightPx();

        assertEquals(laidOutWidthPx, view.mSendButton.getMeasuredWidth());
        assertEquals(laidOutWidthPx, view.mManageDevicesBlock.getMeasuredWidth());
    }
}
