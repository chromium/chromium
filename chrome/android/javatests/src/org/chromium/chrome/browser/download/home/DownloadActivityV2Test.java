// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.Espresso.pressBack;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.isDialog;
import static android.support.test.espresso.matcher.ViewMatchers.hasSibling;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalToIgnoringCase;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;

import android.os.Handler;
import android.os.Looper;
import android.support.test.espresso.action.ViewActions;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.home.rename.RenameUtils;
import org.chromium.chrome.browser.download.home.toolbar.DownloadHomeToolbar;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.download.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.UiRestriction;

import java.util.HashMap;
import java.util.Map;

/** Tests the DownloadActivity home V2. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class DownloadActivityV2Test extends DummyUiActivityTestCase {
    @Mock
    private Tracker mTracker;
    @Mock
    private SnackbarManager mSnackbarManager;

    private ModalDialogManager.Presenter mAppModalPresenter;

    private ModalDialogManager mModalDialogManager;

    private DownloadManagerCoordinator mDownloadCoordinator;

    private StubbedOfflineContentProvider mStubbedOfflineContentProvider;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        UiUtils.setDisableUrlFormattingForTests(true);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE, true);
        features.put(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING, true);
        features.put(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION, false);
        features.put(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER, false);
        features.put(ChromeFeatureList.DOWNLOAD_RENAME, true);
        features.put(ChromeFeatureList.OFFLINE_HOME, false);
        ChromeFeatureList.setTestFeatures(features);

        mStubbedOfflineContentProvider = new StubbedOfflineContentProvider() {
            @Override
            public void renameItem(ContentId id, String name, Callback<Integer> callback) {
                new Handler(Looper.getMainLooper())
                        .post(() -> callback.onResult(handleRename(name)));
            }
        };

        OfflineContentAggregatorFactory.setOfflineContentProviderForTests(
                mStubbedOfflineContentProvider);

        OfflineItem item0 =
                StubbedProvider.createOfflineItem(0, "20151019 07:26", OfflineItemFilter.PAGE);
        OfflineItem item1 =
                StubbedProvider.createOfflineItem(1, "20151020 07:27", OfflineItemFilter.PAGE);
        OfflineItem item2 =
                StubbedProvider.createOfflineItem(2, "20151021 07:28", OfflineItemFilter.OTHER);
        OfflineItem item3 =
                StubbedProvider.createOfflineItem(3, "20151021 07:29", OfflineItemFilter.OTHER);

        mStubbedOfflineContentProvider.addItem(item0);
        mStubbedOfflineContentProvider.addItem(item1);
        mStubbedOfflineContentProvider.addItem(item2);
        mStubbedOfflineContentProvider.addItem(item3);
    }

    private void setUpUi() {
        FilterCoordinator.setPrefetchUserSettingValueForTesting(true);
        DownloadManagerUiConfig config = new DownloadManagerUiConfig.Builder()
                                                 .setIsOffTheRecord(false)
                                                 .setIsSeparateActivity(true)
                                                 .setUseNewDownloadPath(true)
                                                 .setUseNewDownloadPathThumbnails(true)
                                                 .build();

        mAppModalPresenter = new AppModalPresenter(getActivity());

        mModalDialogManager =
                new ModalDialogManager(mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);

        mDownloadCoordinator = new DownloadManagerCoordinatorImpl(
                getActivity(), config, mSnackbarManager, mModalDialogManager, mTracker);
        getActivity().setContentView(mDownloadCoordinator.getView());

        mDownloadCoordinator.updateForUrl(UrlConstants.DOWNLOADS_URL);
    }

    @Test
    @MediumTest
    public void testLaunchingActivity() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        // Shows activity title.
        onView(withText("Downloads")).check(matches(isDisplayed()));

        // Shows the list items.
        onView(withText("page 1")).check(matches(isDisplayed()));
        onView(withText("page 2")).check(matches(isDisplayed()));
        onView(withText("page 3")).check(matches(isDisplayed()));
        onView(withText("page 4")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testTabsAreShown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });
        checkItemsDisplayed(true, true, true, true);

        Matcher filesTabMatcher = allOf(
                withText(equalToIgnoringCase("My Files")), isDescendantOfA(withId(R.id.tabs)));
        Matcher prefetchTabMatcher = allOf(withText(equalToIgnoringCase("Articles for you")),
                isDescendantOfA(withId(R.id.tabs)));
        onView(filesTabMatcher).check(matches(isDisplayed()));
        onView(prefetchTabMatcher).check(matches(isDisplayed()));

        // Select Articles for you tab, and verify the contents.
        onView(prefetchTabMatcher).perform(ViewActions.click());
        checkItemsDisplayed(false, false, false, false);

        // Select My files tab, and verify the contents.
        onView(filesTabMatcher).perform(ViewActions.click());
        checkItemsDisplayed(true, true, true, true);
    }

    @Test
    @MediumTest
    public void testChipsAreShown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        // Two PAGES and two OTHER files. Should show All, Pages, and Other chips.
        checkItemsDisplayed(true, true, true, true);

        Matcher allChipMatcher = allOf(withText(equalToIgnoringCase("All")),
                isDescendantOfA(withId(R.id.content_container)));
        Matcher pagesChipMatcher = allOf(withText(equalToIgnoringCase("Pages")),
                isDescendantOfA(withId(R.id.content_container)));
        Matcher otherChipMatcher = allOf(withText(equalToIgnoringCase("Other")),
                isDescendantOfA(withId(R.id.content_container)));

        onView(allChipMatcher).check(matches(isDisplayed()));
        onView(pagesChipMatcher).check(matches(isDisplayed()));
        onView(otherChipMatcher).check(matches(isDisplayed()));

        // Select Pages chip, and verify the contents.
        onView(pagesChipMatcher).perform(ViewActions.click());
        checkItemsDisplayed(true, true, false, false);

        // Select Other chip, and verify the contents.
        onView(otherChipMatcher).perform(ViewActions.click());
        checkItemsDisplayed(false, false, true, true);

        // Select All chip, and verify the contents.
        onView(allChipMatcher).perform(ViewActions.click());
        checkItemsDisplayed(true, true, true, true);
    }

    @Test
    @MediumTest
    public void testPrefetchTabEmptyText() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        onView(withId(R.id.empty)).check(matches(not(isDisplayed())));

        // Go to Prefetch tab. It should be empty.
        onView(withText(equalToIgnoringCase("Articles for you")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withText(containsString("Articles appear here"))).check(matches(isDisplayed()));
        onView(withId(R.id.empty)).check(matches(isDisplayed()));

        // Go back to files tab. It shouldn't be empty.
        onView(withText(equalToIgnoringCase("My Files")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withId(R.id.empty)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testAddRemoveItems() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        String storageHeaderText = "Using 1.10 KB of";
        onView(withText(containsString(storageHeaderText))).check(matches(isDisplayed()));

        // Add an item. The new item should be visible and the storage text should be updated.
        OfflineItem item5 = StubbedProvider.createOfflineItem("offline_guid_5",
                "http://stuff_and_things.com", OfflineItemState.COMPLETE, 1024, "page 5",
                "/data/fake_path/Downloads/file_5", System.currentTimeMillis(), 100000,
                OfflineItemFilter.OTHER);

        TestThreadUtils.runOnUiThreadBlocking(() -> mStubbedOfflineContentProvider.addItem(item5));
        onView(withText("page 5")).check(matches(isDisplayed()));
        onView(withText(containsString("Using 2.10 KB of"))).check(matches(isDisplayed()));

        // Delete an item. The item should be gone and the storage text should be updated.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mStubbedOfflineContentProvider.removeItem(item5.id));
        onView(withText("page 5")).check(doesNotExist());
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testShowListItemMenu() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });
        onView(withText("page 3")).check(matches(isDisplayed()));

        // Open menu for a download, it should have share, delete, and rename options.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 3"))))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        onView(withText("Rename")).check(matches(isDisplayed()));
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Share")).check(matches(isDisplayed()));

        // Dismiss the menu by pressing back button.
        pressBack();

        // Open menu for a page download, it should have share, delete, but no rename option.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 1"))))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        onView(withText("Rename")).check(doesNotExist());
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Share")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testShowToolbarMenu() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        // In non-selection state settings, search and close menu should be showing, the selection
        // toolbar should not exist.
        onView(withId(R.id.settings_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.search_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.close_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_number)).check(matches(not(isDisplayed())));
        onView(withId(R.id.selection_mode_share_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_delete_menu_id)).check(doesNotExist());

        // Select an item.
        onView(withText("page 1")).perform(ViewActions.longClick());

        // Selection toolbar should be showing. Settings, search, and close menu should be gone.
        onView(withId(R.id.settings_menu_id)).check(doesNotExist());
        onView(withId(R.id.search_menu_id)).check(doesNotExist());
        onView(withId(R.id.close_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_number)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_share_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_delete_menu_id)).check(matches(isDisplayed()));

        // Deselect the same item.
        onView(withText("page 1")).perform(ViewActions.longClick());

        // The toolbar should flip back to non-selection state.
        onView(withId(R.id.settings_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.search_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.close_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_number)).check(matches(not(isDisplayed())));
        onView(withId(R.id.selection_mode_share_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_delete_menu_id)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testDeleteItem() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });
        SnackbarManager.setDurationForTesting(1);

        onView(withText("page 1")).check(matches(isDisplayed()));

        // Delete an item using three dot menu. The item should be removed from the list.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 1"))))
                .perform(ViewActions.click());
        onView(withText("Delete")).check(matches(isDisplayed())).perform(ViewActions.click());
        onView(withText("page 1")).check(doesNotExist());

        // Delete the remaining items using long press and multi-delete from toolbar menu.
        onView(withText("page 2")).check(matches(isDisplayed())).perform(ViewActions.longClick());
        onView(withText("page 3")).check(matches(isDisplayed())).perform(ViewActions.longClick());
        onView(withText("page 4")).check(matches(isDisplayed())).perform(ViewActions.longClick());

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            DownloadHomeToolbar toolbar = getActivity().findViewById(R.id.download_toolbar);
            toolbar.getMenu().performIdentifierAction(R.id.selection_mode_delete_menu_id, 0);
        });

        // The files tab should show empty view now.
        onView(withId(R.id.empty)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testRenameItem() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        RenameUtils.disableNativeForTesting();

        // Rename a non-offline-page item using three dot menu.
        // Open menu for a list item, it should have the rename option.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 4"))))
                .perform(ViewActions.click());
        // Rename an item. The rename dialog should popup.
        onView(withText("Rename")).check(matches(isDisplayed())).perform(ViewActions.click());

        // Test rename dialog with error message.
        renameFileAndVerifyErrorMessage("name_conflict", R.string.rename_failure_name_conflict);
        renameFileAndVerifyErrorMessage("name_too_long", R.string.rename_failure_name_too_long);
        renameFileAndVerifyErrorMessage("name_invalid", R.string.rename_failure_name_invalid);
        renameFileAndVerifyErrorMessage("rename_unavailable", R.string.rename_failure_unavailable);

        // Test empty input.
        onView(withId(R.id.file_name)).inRoot(isDialog()).perform(ViewActions.clearText());
        onView(withText("OK")).inRoot(isDialog()).check(matches(not(isEnabled())));

        // Test successful commit.
        renameFileAndVerifyErrorMessage("rename_file_successful", -1);

        // TODO(hesen): Test rename extension dialog.
    }

    @Test
    @MediumTest
    public void testShareItem() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        // Open menu for a list item, it should have the share option.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 4"))))
                .perform(ViewActions.click());

        // Share an item. The share via android dialog should popup.
        onView(withText("Share")).check(matches(isDisplayed())).perform(ViewActions.click());

        // TODO(shaktisahu): Test content of the intent.
    }

    @Test
    @MediumTest
    public void testSearchView() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        final DownloadHomeToolbar toolbar = getActivity().findViewById(R.id.download_toolbar);
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0));

        // The selection should be cleared when a search is started.
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Select an item and assert that the search view is no longer showing.
        onView(withText("page 4")).perform(ViewActions.longClick());
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        // Clear the selection and assert that the search view is showing again.
        onView(withText("page 4")).perform(ViewActions.longClick());
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Close the search view, by clicking back button on toolbar.
        onView(withContentDescription("Go back")).perform(ViewActions.click());
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));
    }

    private void checkItemsDisplayed(boolean item0, boolean item1, boolean item2, boolean item3) {
        onView(withText("page 1")).check(item0 ? matches(isDisplayed()) : doesNotExist());
        onView(withText("page 2")).check(item1 ? matches(isDisplayed()) : doesNotExist());
        onView(withText("page 3")).check(item2 ? matches(isDisplayed()) : doesNotExist());
        onView(withText("page 4")).check(item3 ? matches(isDisplayed()) : doesNotExist());
    }

    private void renameFileAndVerifyErrorMessage(String name, int expectErrorMsgId) {
        onView(withId(R.id.file_name))
                .inRoot(isDialog())
                .perform(ViewActions.clearText())
                .perform(ViewActions.typeText(name));

        onView(withText("OK"))
                .inRoot(isDialog())
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        if (expectErrorMsgId != -1) {
            onView(withText(getActivity().getResources().getString(expectErrorMsgId)))
                    .inRoot(isDialog())
                    .check(matches(isDisplayed()));
        }
    }

    private int /*@RenameResult*/ handleRename(String name) {
        int result = RenameResult.SUCCESS;
        switch (name) {
            case "name_conflict":
                result = RenameResult.FAILURE_NAME_CONFLICT;
                break;
            case "name_too_long":
                result = RenameResult.FAILURE_NAME_TOO_LONG;
                break;
            case "name_invalid":
                result = RenameResult.FAILURE_NAME_INVALID;
                break;
            case "rename_unavailable":
                result = RenameResult.FAILURE_UNAVAILABLE;
                break;
            default:
                break;
        }
        return result;
    }
}
