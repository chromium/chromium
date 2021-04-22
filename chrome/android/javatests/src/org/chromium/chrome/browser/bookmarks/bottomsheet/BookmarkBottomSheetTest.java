// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

import static org.chromium.components.feature_engagement.FeatureConstants.READ_LATER_BOTTOM_SHEET_FEATURE;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;

/**
 * Test to verify bookmark bottom sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.BOOKMARK_BOTTOM_SHEET, ChromeFeatureList.READ_LATER})
public class BookmarkBottomSheetTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TITLE = "bookmark title";
    private static final GURL TEST_URL_A = new GURL("http://a.com");
    private static final GURL TEST_URL_B = new GURL("http://b.com");
    private static final String READING_LIST_TITLE = "Reading list ";
    private static final String READING_LIST_TITLE_NEW = "Reading list New";

    private BookmarkBottomSheetCoordinator mBottomSheetCoordinator;
    private BottomSheetController mBottomSheetController;
    private BookmarkModel mBookmarkModel;
    private BookmarkItem mItemClicked;
    private boolean mCallbackInvoked;
    @Mock
    private Tracker mTracker;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUI(READ_LATER_BOTTOM_SHEET_FEATURE)).thenReturn(false);

        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel = new BookmarkModel(Profile.fromWebContents(
                    mActivityTestRule.getActivity().getActivityTab().getWebContents()));
            TrackerFactory.setTrackerForTests(mTracker);
        });

        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        mBottomSheetCoordinator = new BookmarkBottomSheetCoordinator(
                mActivityTestRule.getActivity(), mBottomSheetController, mBookmarkModel);
        waitForBookmarkModelLoaded();
    }

    @After
    public void tearDown() {
        mItemClicked = null;
    }

    private void setShouldShowNew(boolean shouldShowNew) {
        when(mTracker.shouldTriggerHelpUI(READ_LATER_BOTTOM_SHEET_FEATURE))
                .thenReturn(shouldShowNew);
    }

    private void bottomSheetCallback(BookmarkItem item) {
        mItemClicked = item;
        mCallbackInvoked = true;
    }

    private void showBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheetCoordinator.show(this::bottomSheetCallback); });

        CriteriaHelper.pollUiThread(
                () -> mBottomSheetController.getSheetState() == SheetState.FULL);
    }

    private void hideBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetCoordinator.getBottomSheetContentForTesting(), false);
        });

        CriteriaHelper.pollUiThread(
                () -> mBottomSheetController.getSheetState() == SheetState.HIDDEN);
    }

    private void waitForBookmarkModelLoaded() {
        // Must load partner bookmark backend, or the model will not be loaded.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting());
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    private void waitForBookmarkClicked() {
        CriteriaHelper.pollUiThread(() -> mItemClicked != null);
    }

    private RecyclerView getRecyclerView() {
        BottomSheetContent content = mBottomSheetCoordinator.getBottomSheetContentForTesting();
        return content.getContentView().findViewById(org.chromium.chrome.R.id.sheet_item_list);
    }

    private void assertNoOverflowMenu(int position, String message) {
        View view = getRecyclerView().findViewHolderForAdapterPosition(position).itemView;
        Assert.assertEquals(message, View.GONE, view.findViewById(R.id.more).getVisibility());
    }

    private void assertViewHolderHasString(int position, String expectedString) {
        View view = getRecyclerView().findViewHolderForAdapterPosition(position).itemView;
        ArrayList<View> views = new ArrayList<>();
        view.findViewsWithText(views, expectedString, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertFalse(views.isEmpty());
    }

    @Test
    @MediumTest
    public void testBottomSheetShowWithoutBookmarks() throws InterruptedException {
        showBottomSheet();
        onView(withId(R.id.sheet_title)).check(matches(isDisplayed()));
        onView(withText(READING_LIST_TITLE)).check(matches(isDisplayed()));
        assertViewHolderHasString(0, "Save this page for later and get a reminder");
        assertNoOverflowMenu(0, "No overflow menu for reading list folder.");

        onView(withText("Mobile bookmarks")).check(matches(isDisplayed()));
        assertViewHolderHasString(1, "No bookmarks");
        assertNoOverflowMenu(1, "No overflow menu for mobile bookmark folder.");
    }

    @Test
    @MediumTest
    public void testBottomSheetNewIPH() {
        setShouldShowNew(true);
        showBottomSheet();
        onView(withText(READING_LIST_TITLE_NEW)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testBottomSheetShowWithOneBookmark() {
        // Add 1 bookmark and 1 unread page.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addBookmark(mBookmarkModel.getMobileFolderId(), 0, TITLE, TEST_URL_A);
            mBookmarkModel.addToReadingList(TITLE, TEST_URL_A);
        });

        showBottomSheet();
        onView(withText(READING_LIST_TITLE)).check(matches(isDisplayed()));
        assertViewHolderHasString(0, "1 unread page");

        onView(withText("Mobile bookmarks")).check(matches(isDisplayed()));
        assertViewHolderHasString(1, "1 bookmark");
    }

    @Test
    @MediumTest
    public void testBottomSheetShowWithBookmarks() {
        // Add multiple bookmarks unread reading list pages.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addBookmark(mBookmarkModel.getMobileFolderId(), 0, TITLE, TEST_URL_A);
            mBookmarkModel.addBookmark(mBookmarkModel.getMobileFolderId(), 0, TITLE, TEST_URL_B);
            mBookmarkModel.addToReadingList(TITLE, TEST_URL_A);
            mBookmarkModel.addToReadingList(TITLE, TEST_URL_B);
        });

        showBottomSheet();
        onView(withText(READING_LIST_TITLE)).check(matches(isDisplayed()));
        assertViewHolderHasString(0, "2 unread pages");

        onView(withText("Mobile bookmarks")).check(matches(isDisplayed()));
        assertViewHolderHasString(1, "2 bookmarks");
    }

    @Test
    @MediumTest
    public void testBottomSheetClickThrough() {
        showBottomSheet();
        onView(withText("Mobile bookmarks")).check(matches(isDisplayed()));
        onView(withText(READING_LIST_TITLE)).check(matches(isDisplayed())).perform(click());

        waitForBookmarkClicked();
        Assert.assertEquals(BookmarkType.READING_LIST, mItemClicked.getId().getType());
        Assert.assertTrue(mItemClicked.isFolder());
    }

    @Test
    @MediumTest
    public void testBottomSheetCloseInvokeCallback() {
        showBottomSheet();
        onView(withText(READING_LIST_TITLE)).check(matches(isDisplayed()));
        hideBottomSheet();
        Assert.assertNull(mItemClicked);
        Assert.assertTrue(mCallbackInvoked);
    }
}
