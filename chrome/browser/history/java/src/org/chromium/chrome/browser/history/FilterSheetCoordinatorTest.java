// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.history.FilterSheetCoordinator.MAX_SHEET_HEIGHT_RATIO;
import static org.chromium.chrome.browser.history.FilterSheetCoordinator.MAX_VISIBLE_ITEM_COUNT;

import android.graphics.drawable.Drawable;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the filter sheet for history page. */
@RunWith(BaseRobolectricTestRunner.class)
public class FilterSheetCoordinatorTest {
    private static final String APPID_YOUTUBE = "com.google.android.youtube";
    private static final String APPID_CHROME = "com.android.chrome";
    private static final String APPID_CALENDAR = "com.google.android.calendar";
    private static final String APPID_MESSAGE = "com.google.android.apps.messaging";

    private static final CharSequence APPLABEL_YOUTUBE = "YouTube";
    private static final CharSequence APPLABEL_CHROME = "Chrome";
    private static final CharSequence APPLABEL_CALENDAR = "Calendar";
    private static final CharSequence APPLABEL_MESSAGE = "Message";

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    private FilterSheetCoordinator mFilterSheet;
    private FilterItem mCurrentItem;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Drawable icon =
                                    activity.getResources().getDrawable(R.drawable.ic_devices_16dp);
                            List<FilterItem> items = new ArrayList<>();
                            items.add(new FilterItem(APPID_YOUTUBE, icon, APPLABEL_YOUTUBE));
                            items.add(new FilterItem(APPID_CHROME, icon, APPLABEL_CHROME));
                            items.add(new FilterItem(APPID_CALENDAR, icon, APPLABEL_CALENDAR));
                            items.add(new FilterItem(APPID_MESSAGE, icon, APPLABEL_MESSAGE));
                            mFilterSheet =
                                    new FilterSheetCoordinator(
                                            activity,
                                            activity.getWindow().getDecorView(),
                                            mBottomSheetController,
                                            this::onFilterItemUpdated,
                                            items,
                                            R.string.history_filter_by_app);
                        });
    }

    private void onFilterItemUpdated(FilterItem filterItem) {
        mCurrentItem = filterItem;
    }

    private void setCurrentFilterItem(String id, CharSequence label) {
        mCurrentItem = id == null ? null : new FilterItem(id, null, label);
    }

    private int calcSheetHeight(int rowHeight, int baseViewHeight, int rowCount) {
        return FilterSheetCoordinator.calculateSheetHeight(rowHeight, baseViewHeight, rowCount);
    }

    @Test
    public void testSheetSize() {
        final int rowHeight = 64;
        final int baseHeight = 1200;
        final int defaultMaxHeight = rowHeight * MAX_VISIBLE_ITEM_COUNT;

        int rowCount = MAX_VISIBLE_ITEM_COUNT - 1;
        assertEquals(
                rowHeight * ((long) rowCount), calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT;
        assertEquals(
                rowHeight * ((long) rowCount), calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT + 1;
        assertEquals(defaultMaxHeight, calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT * 2;
        assertEquals(defaultMaxHeight, calcSheetHeight(rowHeight, baseHeight, rowCount));

        final int smallBase = 300;
        final int maxHeight = (int) (smallBase * MAX_SHEET_HEIGHT_RATIO);

        rowCount = 2;
        assertEquals(
                rowHeight * ((long) rowCount), calcSheetHeight(rowHeight, smallBase, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT;
        assertEquals(maxHeight, calcSheetHeight(rowHeight, smallBase, rowCount));

        rowCount = 100;
        assertEquals(maxHeight, calcSheetHeight(rowHeight, smallBase, rowCount));
    }

    @Test
    public void testFullHistoryToApp() {
        assertEquals("Selected app is not correct.", null, mCurrentItem);

        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickItemForTesting(APPID_MESSAGE);

        // Tapping an app selects it.
        assertEquals("Chosen app is not correct.", APPID_MESSAGE, mCurrentItem.id);
        assertEquals("Chosen label is not correct.", APPLABEL_MESSAGE, mCurrentItem.label);
    }

    @Test
    public void testSelectNewApp() {
        setCurrentFilterItem(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mCurrentItem.id);
        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickItemForTesting(APPID_CHROME);

        // Tapping an app makes it a newly selected one.
        assertEquals("Chosen app is not correct.", APPID_CHROME, mCurrentItem.id);
        assertEquals("Chosen label is not correct.", APPLABEL_CHROME, mCurrentItem.label);
    }

    @Test
    public void testUnselectApp() {
        setCurrentFilterItem(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mCurrentItem.id);

        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickItemForTesting(APPID_CALENDAR);

        // Tapping the already selected app unselects it.
        assertEquals("Chosen app is not correct.", null, mCurrentItem);

        // Open the sheet once more and select the app that was unselected right before.
        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickItemForTesting(APPID_CALENDAR);
        assertEquals("Chosen app is not correct.", APPID_CALENDAR, mCurrentItem.id);
        assertEquals("Chosen label is not correct.", APPLABEL_CALENDAR, mCurrentItem.label);
    }

    @Test
    public void testResetSheetAtOpen() {
        assertEquals("Selected app is not correct.", null, mCurrentItem);

        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickItemForTesting(APPID_CALENDAR);
        assertEquals("Chosen app should be Calendar.", APPID_CALENDAR, mCurrentItem.id);

        // Caller resets its state and opens the sheet again. The sheet should be reset in sync.
        setCurrentFilterItem(null, null);
        mFilterSheet.openSheet(mCurrentItem);
        assertEquals("No app should be selected.", null, mFilterSheet.getCurrentItemIdForTesting());

        setCurrentFilterItem(APPID_YOUTUBE, APPLABEL_YOUTUBE);
        mFilterSheet.openSheet(mCurrentItem);
        assertEquals(
                "Chosen app should be YouTube.",
                APPID_YOUTUBE,
                mFilterSheet.getCurrentItemIdForTesting());
    }

    @Test
    public void testCloseSheetWithoutSelection() {
        setCurrentFilterItem(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mCurrentItem.id);

        mFilterSheet.openSheet(mCurrentItem);
        mFilterSheet.clickCloseButtonForTesting();

        // Closing the sheet preserves the previously selected app.
        assertEquals("Chosen app is not correct.", APPID_CALENDAR, mCurrentItem.id);
        assertEquals("Chosen label is not correct.", APPLABEL_CALENDAR, mCurrentItem.label);
    }

    @Test
    public void testUpdateItems() {
        List<FilterItem> updatedApps = new ArrayList<>();
        updatedApps.add(new FilterItem("com.example.newapp", null, "New App"));

        mFilterSheet.updateItems(updatedApps);
        mFilterSheet.openSheet(null);
        mFilterSheet.clickItemForTesting("com.example.newapp");

        assertEquals("Chosen app is not correct.", "com.example.newapp", mCurrentItem.id);
        assertEquals("Chosen label is not correct.", "New App", mCurrentItem.label);
    }
}
