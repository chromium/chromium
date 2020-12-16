// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;
import android.view.KeyEvent;
import android.widget.ListView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gesturenav.NavigationSheetMediator.ItemProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.mock.MockNavigationController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the gesture navigation sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationSheetTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int INVALID_NAVIGATION_INDEX = -1;
    private static final int NAVIGATION_INDEX_1 = 1;
    private static final int NAVIGATION_INDEX_2 = 5;

    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
    }

    private static class TestNavigationEntry extends NavigationEntry {
        public TestNavigationEntry(int index, GURL url, GURL virtualUrl, GURL originalUrl,
                String title, Bitmap favicon, int transition, long timestamp) {
            super(index, url, virtualUrl, originalUrl, /*referrerUrl=*/null, title, favicon,
                    transition, timestamp);
        }
    }

    private static class TestNavigationController extends MockNavigationController {
        private final NavigationHistory mHistory;
        private int mNavigatedIndex = INVALID_NAVIGATION_INDEX;

        public TestNavigationController() {
            mHistory = new NavigationHistory();
            mHistory.addEntry(new TestNavigationEntry(NAVIGATION_INDEX_1, new GURL("about:blank"),
                    GURL.emptyGURL(), GURL.emptyGURL(), "About Blank", null, 0, 0));
            mHistory.addEntry(new TestNavigationEntry(NAVIGATION_INDEX_2,
                    new GURL(UrlUtils.encodeHtmlDataUri("<html>1</html>")), GURL.emptyGURL(),
                    GURL.emptyGURL(), null, null, 0, 0));
        }

        @Override
        public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
            return mHistory;
        }

        @Override
        public void goToNavigationIndex(int index) {
            mNavigatedIndex = index;
        }
    }

    private class TestSheetDelegate implements NavigationSheet.Delegate {
        private static final int MAXIMUM_HISTORY_ITEMS = 8;

        private final NavigationController mNavigationController;

        public TestSheetDelegate(NavigationController controller) {
            mNavigationController = controller;
        }

        @Override
        public NavigationHistory getHistory(boolean forward) {
            return mNavigationController.getDirectedNavigationHistory(
                    forward, MAXIMUM_HISTORY_ITEMS);
        }

        @Override
        public void navigateToIndex(int index) {
            mNavigationController.goToNavigationIndex(index);
        }
    }

    private NavigationSheet getNavigationSheet() {
        RootUiCoordinator coordinator =
                mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        return ((TabbedRootUiCoordinator) coordinator).getNavigationSheetForTesting();
    }

    @Test
    @MediumTest
    public void testFaviconFetching() throws ExecutionException {
        TestNavigationController controller = new TestNavigationController();
        NavigationSheetCoordinator sheet = (NavigationSheetCoordinator) showPopup(controller);
        ListView listview = sheet.getContentView().findViewById(R.id.navigation_entries);

        CriteriaHelper.pollUiThread(() -> {
            for (int i = 0; i < controller.mHistory.getEntryCount(); i++) {
                ListItem item = (ListItem) listview.getAdapter().getItem(i);
                Criteria.checkThat(i + "th element", item.model.get(ItemProperties.ICON),
                        Matchers.notNullValue());
            }
        });
    }

    @Test
    @SmallTest
    public void testItemSelection() throws ExecutionException {
        TestNavigationController controller = new TestNavigationController();
        NavigationSheetCoordinator sheet = (NavigationSheetCoordinator) showPopup(controller);
        ListView listview = sheet.getContentView().findViewById(R.id.navigation_entries);

        CriteriaHelper.pollUiThread(() -> listview.getChildCount() >= 2);
        Assert.assertEquals(INVALID_NAVIGATION_INDEX, controller.mNavigatedIndex);

        ThreadUtils.runOnUiThreadBlocking(() -> listview.getChildAt(1).callOnClick());

        CriteriaHelper.pollUiThread(sheet::isHidden);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(controller.mNavigatedIndex, Matchers.is(NAVIGATION_INDEX_2));
        });
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testLongPressBackTriggering() {
        KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.onKeyDown(KeyEvent.KEYCODE_BACK, event); });
        CriteriaHelper.pollUiThread(activity::hasPendingNavigationRunnableForTesting);

        // Wait for the long press timeout to trigger and show the navigation popup.
        CriteriaHelper.pollUiThread(() -> getNavigationSheet() != null);
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testLongPressBackTriggering_Cancellation() throws ExecutionException {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
            activity.onKeyDown(KeyEvent.KEYCODE_BACK, event);
        });
        CriteriaHelper.pollUiThread(activity::hasPendingNavigationRunnableForTesting);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            KeyEvent event = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK);
            activity.onKeyUp(KeyEvent.KEYCODE_BACK, event);
        });
        CriteriaHelper.pollUiThread(() -> !activity.hasPendingNavigationRunnableForTesting());

        // Ensure no navigation popup is showing.
        Assert.assertNull(TestThreadUtils.runOnUiThreadBlocking(this::getNavigationSheet));
    }

    private NavigationSheet showPopup(NavigationController controller) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTabProvider().get();
            NavigationSheet navigationSheet =
                    NavigationSheet.create(tab.getContentView(), mActivityTestRule.getActivity(),
                            () -> mBottomSheetController, Profile.getLastUsedRegularProfile());
            navigationSheet.setDelegate(new TestSheetDelegate(controller));
            navigationSheet.startAndExpand(false, false);
            return navigationSheet;
        });
    }
}
