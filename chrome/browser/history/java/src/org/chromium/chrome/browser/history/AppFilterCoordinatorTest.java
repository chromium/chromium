// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.history.AppFilterCoordinator.MAX_SHEET_HEIGHT_RATIO;
import static org.chromium.chrome.browser.history.AppFilterCoordinator.MAX_VISIBLE_ITEM_COUNT;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.List;

/** Integration tests for the app filter sheet for history page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppFilterCoordinatorTest {
    /** {@link AppInfo} indicating that no app is selected i.e. full history. */
    private static final AppInfo APP_NOTSELECTED = new AppInfo(null, null, null);

    private static final String APPID_YOUTUBE = "com.google.android.youtube";
    private static final String APPID_CHROME = "com.android.chrome";
    private static final String APPID_CALENDAR = "com.google.android.calendar";
    private static final String APPID_MESSAGE = "com.google.android.apps.messaging";

    private static final CharSequence APPLABEL_YOUTUBE = "YouTube";
    private static final CharSequence APPLABEL_CHROME = "Chrome";
    private static final CharSequence APPLABEL_CALENDAR = "Calendar";
    private static final CharSequence APPLABEL_MESSAGE = "Message";

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private BottomSheetController mBottomSheetController;
    private AppFilterCoordinator mAppFilterSheet;
    private Drawable mIcon;
    private String mAppId;
    private CharSequence mAppLabel;

    @Before
    public void setUp() throws InterruptedException {
        mActivityRule.launchActivity(null);
        Activity activity = getActivity();
        ApplicationTestUtils.waitForActivityState(activity, Stage.RESUMED);
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController = createBottomSheetController();

                    mIcon = activity.getResources().getDrawable(R.drawable.ic_devices_16dp);
                    List<AppInfo> apps = new ArrayList<>();
                    apps.add(new AppInfo(APPID_YOUTUBE, mIcon, APPLABEL_YOUTUBE));
                    apps.add(new AppInfo(APPID_CHROME, mIcon, APPLABEL_CHROME));
                    apps.add(new AppInfo(APPID_CALENDAR, mIcon, APPLABEL_CALENDAR));
                    apps.add(new AppInfo(APPID_MESSAGE, mIcon, APPLABEL_MESSAGE));
                    mAppFilterSheet =
                            new AppFilterCoordinator(
                                    activity,
                                    activity.getWindow().getDecorView(),
                                    mBottomSheetController,
                                    this::onAppUpdated,
                                    apps);
                });
    }

    private BlankUiTestActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private BottomSheetController createBottomSheetController() {
        ViewGroup activityContentView = getActivity().findViewById(android.R.id.content);
        ScrimCoordinator scrimCoordinator =
                new ScrimCoordinator(
                        getActivity(),
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        activityContentView,
                        Color.WHITE);
        return BottomSheetControllerFactory.createBottomSheetController(
                () -> scrimCoordinator,
                (unused) -> {},
                getActivity().getWindow(),
                KeyboardVisibilityDelegate.getInstance(),
                () -> activityContentView,
                () -> 0);
    }

    private void onAppUpdated(AppInfo appInfo) {
        mAppId = appInfo != null ? appInfo.id : null;
        mAppLabel = appInfo != null ? appInfo.label : null;
    }

    private void setCurrentAppInfo(String appId, CharSequence appLabel) {
        mAppId = appId;
        mAppLabel = appLabel;
        mAppFilterSheet.setCurrentAppForTesting(appId);
    }

    private int calcSheetHeight(int rowHeight, int baseViewHeight, int rowCount) {
        return AppFilterCoordinator.calculateSheetHeight(rowHeight, baseViewHeight, rowCount);
    }

    @Test
    @SmallTest
    public void testSheetSize() {
        final int rowHeight = 64;
        final int baseHeight = 1200;
        final int defaultMaxHeight = rowHeight * MAX_VISIBLE_ITEM_COUNT;

        int rowCount = MAX_VISIBLE_ITEM_COUNT - 1;
        assertEquals(rowHeight * rowCount, calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT;
        assertEquals(rowHeight * rowCount, calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT + 1;
        assertEquals(defaultMaxHeight, calcSheetHeight(rowHeight, baseHeight, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT * 2;
        assertEquals(defaultMaxHeight, calcSheetHeight(rowHeight, baseHeight, rowCount));

        final int smallBase = 300;
        final int maxHeight = (int) (smallBase * MAX_SHEET_HEIGHT_RATIO);

        rowCount = 2;
        assertEquals(rowHeight * rowCount, calcSheetHeight(rowHeight, smallBase, rowCount));

        rowCount = MAX_VISIBLE_ITEM_COUNT;
        assertEquals(maxHeight, calcSheetHeight(rowHeight, smallBase, rowCount));

        rowCount = 100;
        assertEquals(maxHeight, calcSheetHeight(rowHeight, smallBase, rowCount));
    }

    @Test
    @MediumTest
    public void testFullHistoryToApp() {
        assertEquals("Selected app is not correct.", null, mAppId);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppFilterSheet.openSheet();
                    mAppFilterSheet.clickItemForTesting(APPID_MESSAGE);
                });

        // Tapping an app selects it.
        assertEquals("Chosen app is not correct.", APPID_MESSAGE, mAppId);
        assertEquals("Chosen app is not correct.", APPLABEL_MESSAGE, mAppLabel);
    }

    @Test
    @MediumTest
    public void testSelectNewApp() {
        setCurrentAppInfo(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mAppId);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppFilterSheet.openSheet();
                    mAppFilterSheet.clickItemForTesting(APPID_CHROME);
                });

        // Tapping an app makes it a newly selected one.
        assertEquals("Chosen app is not correct.", APPID_CHROME, mAppId);
        assertEquals("Chosen app is not correct.", APPLABEL_CHROME, mAppLabel);
    }

    @Test
    @MediumTest
    public void testUnselectApp() {
        setCurrentAppInfo(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mAppId);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppFilterSheet.openSheet();
                    mAppFilterSheet.clickItemForTesting(APPID_CALENDAR);
                });

        // Tapping the already selected app unselects it.
        assertEquals("Chosen app is not correct.", APP_NOTSELECTED.id, mAppId);
        assertEquals("Chosen app is not correct.", APP_NOTSELECTED.label, mAppLabel);
    }

    @Test
    @MediumTest
    public void testCloseSheetWithoutSelection() {
        setCurrentAppInfo(APPID_CALENDAR, APPLABEL_CALENDAR);
        assertEquals("Selected app is not correct.", APPID_CALENDAR, mAppId);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppFilterSheet.openSheet();
                    mAppFilterSheet.clickCloseButtonForTesting();
                });

        // Closing the sheet preserves the previously selected app.
        assertEquals("Chosen app is not correct.", APPID_CALENDAR, mAppId);
        assertEquals("Chosen app is not correct.", APPLABEL_CALENDAR, mAppLabel);
    }
}
