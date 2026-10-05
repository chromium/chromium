// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for {@link HistoryFilterChip}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoryFilterChipUnitTest {
    private static final FilterItem ITEM_1 = new FilterItem("id_1", null, "Label 1");
    private static final FilterItem ITEM_2 = new FilterItem("id_2", null, "Label 2");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<FilterItem> mOnSelectionChanged;
    @Mock private Runnable mOnSheetOpened;
    @Mock private Runnable mHideSoftKeyboard;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FilterSheetCoordinator mFilterSheet;

    private Activity mActivity;
    private ViewGroup mContainer;
    private HistoryFilterChip mChip;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContainer =
                (ViewGroup)
                        LayoutInflater.from(mActivity).inflate(R.layout.history_filter_chips, null);
        mChip =
                new HistoryFilterChip(
                        R.id.app_history_filter_chip,
                        R.string.history_filter_by_app,
                        /* canShow= */ true,
                        mOnSelectionChanged,
                        mOnSheetOpened);
        mChip.bindView(
                mContainer, mActivity, SupplierUtils.of(mBottomSheetController), mHideSoftKeyboard);
    }

    @Test
    public void testInitialState() {
        ChipView chipView = mChip.getChipViewForTesting();
        assertFalse(mChip.isVisible());
        assertEquals(View.GONE, chipView.getVisibility());
        assertFalse(chipView.isSelected());
        assertEquals(
                mActivity.getString(R.string.history_filter_by_app),
                chipView.getPrimaryTextView().getText().toString());
        assertNull(mChip.getSelectedItemForTesting());
        assertTrue(mChip.getItemsForTesting().isEmpty());
    }

    @Test
    public void testVisibility_RequiresAtLeastTwoItemsOrSelectedItem() {
        ChipView chipView = mChip.getChipViewForTesting();

        mChip.setItems(List.of(ITEM_1));
        assertFalse(mChip.isVisible());
        assertEquals(View.GONE, chipView.getVisibility());

        mChip.setItems(List.of(ITEM_1, ITEM_2));
        assertTrue(mChip.isVisible());
        assertEquals(View.VISIBLE, chipView.getVisibility());

        // When an item is selected, the chip stays visible even if items drop below 2.
        mChip.onItemSelected(ITEM_1);
        mChip.setItems(List.of(ITEM_1));
        assertTrue(mChip.isVisible());
        assertEquals(View.VISIBLE, chipView.getVisibility());

        // Clearing the selection hides the chip when fewer than 2 items remain.
        mChip.onItemSelected(null);
        assertFalse(mChip.isVisible());
        assertEquals(View.GONE, chipView.getVisibility());
    }

    @Test
    public void testVisibility_CanShowFalse() {
        HistoryFilterChip disabledChip =
                new HistoryFilterChip(
                        R.id.host_history_filter_chip,
                        R.string.history_filter_by_host,
                        /* canShow= */ false,
                        mOnSelectionChanged,
                        /* onSheetOpened= */ null);
        disabledChip.bindView(
                mContainer,
                mActivity,
                /* bottomSheetControllerSupplier= */ null,
                /* hideSoftKeyboard= */ null);
        ChipView chipView = disabledChip.getChipViewForTesting();

        assertFalse(disabledChip.isVisible());
        assertEquals(View.GONE, chipView.getVisibility());
    }

    @Test
    public void testOnItemSelected_UpdatesChipAndNotifiesCallback() {
        ChipView chipView = mChip.getChipViewForTesting();
        mChip.setItems(List.of(ITEM_1, ITEM_2));

        mChip.onItemSelected(ITEM_1);
        assertEquals(ITEM_1, mChip.getSelectedItemForTesting());
        assertTrue(chipView.isSelected());
        assertEquals("Label 1", chipView.getPrimaryTextView().getText().toString());
        verify(mOnSelectionChanged, times(1)).onResult(ITEM_1);

        // Selecting the same item again is a no-op.
        mChip.onItemSelected(ITEM_1);
        verify(mOnSelectionChanged, times(1)).onResult(ITEM_1);

        // Selecting a different item updates the label and notifies the callback.
        mChip.onItemSelected(ITEM_2);
        assertEquals(ITEM_2, mChip.getSelectedItemForTesting());
        assertTrue(chipView.isSelected());
        assertEquals("Label 2", chipView.getPrimaryTextView().getText().toString());
        verify(mOnSelectionChanged).onResult(ITEM_2);

        // Unselecting resets the chip view and notifies the callback with null.
        mChip.onItemSelected(null);
        assertNull(mChip.getSelectedItemForTesting());
        assertFalse(chipView.isSelected());
        assertEquals(
                mActivity.getString(R.string.history_filter_by_app),
                chipView.getPrimaryTextView().getText().toString());
        verify(mOnSelectionChanged).onResult(null);
    }

    @Test
    public void testReset_ClearsSelectionWithoutCallback() {
        ChipView chipView = mChip.getChipViewForTesting();
        mChip.setItems(List.of(ITEM_1));
        mChip.onItemSelected(ITEM_1);
        assertTrue(mChip.isVisible());
        assertTrue(chipView.isSelected());

        mChip.reset();
        assertNull(mChip.getSelectedItemForTesting());
        assertFalse(chipView.isSelected());
        assertEquals(
                mActivity.getString(R.string.history_filter_by_app),
                chipView.getPrimaryTextView().getText().toString());
        assertFalse(mChip.isVisible());
        assertEquals(View.GONE, chipView.getVisibility());
        verify(mOnSelectionChanged, never()).onResult(null);
    }

    @Test
    public void testSetItems_UpdatesFilterSheetWhenPresent() {
        mChip.setFilterSheetForTesting(mFilterSheet);
        List<FilterItem> items = List.of(ITEM_1, ITEM_2);

        mChip.setItems(items);
        assertEquals(items, mChip.getItemsForTesting());
        verify(mFilterSheet).updateItems(items);
    }

    @Test
    public void testClickChip_OpensSheetAndHidesKeyboard() {
        mChip.setFilterSheetForTesting(mFilterSheet);
        mChip.setItems(List.of(ITEM_1, ITEM_2));
        mChip.onItemSelected(ITEM_1);

        mChip.getChipViewForTesting().performClick();

        verify(mHideSoftKeyboard).run();
        verify(mFilterSheet).openSheet(ITEM_1);
        verify(mOnSheetOpened).run();
    }

    @Test
    public void testClickChip_LazyInitializesFilterSheet() {
        mChip.setItems(List.of(ITEM_1, ITEM_2));

        mChip.getChipViewForTesting().performClick();

        verify(mHideSoftKeyboard).run();
        verify(mBottomSheetController).requestShowContent(any(FilterSheetContent.class), eq(true));
        verify(mOnSheetOpened).run();
    }

    @Test
    public void testSetChipEnabled() {
        ChipView chipView = mChip.getChipViewForTesting();
        assertTrue(chipView.isEnabled());

        mChip.setChipEnabled(false);
        assertFalse(chipView.isEnabled());

        mChip.setChipEnabled(true);
        assertTrue(chipView.isEnabled());

        // Calling setChipEnabled before bindView should also be applied when bound.
        HistoryFilterChip unboundChip =
                new HistoryFilterChip(
                        R.id.host_history_filter_chip,
                        R.string.history_filter_by_host,
                        /* canShow= */ true,
                        mOnSelectionChanged,
                        /* onSheetOpened= */ null);
        unboundChip.setChipEnabled(false);
        unboundChip.bindView(
                mContainer, mActivity, SupplierUtils.of(mBottomSheetController), mHideSoftKeyboard);
        assertFalse(unboundChip.getChipViewForTesting().isEnabled());
    }
}
