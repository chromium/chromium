// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightOff;
import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightPulse;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Build;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerProperties;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerTestingDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkToolbar;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRow;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.ViewType;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/** Tests for the bookmark manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP})
@ImportantFormFactors(DeviceFormFactor.ONLY_TABLET)
// TODO(crbug.com/40899175): Investigate batching.
@DoNotBatch(reason = "BookmarkTest has behaviours and thus can't be batched.")
public class BookmarkTest {
    private static final String TEST_PAGE_URL_GOOGLE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final String TEST_PAGE_TITLE_GOOGLE2 = "Google";
    private static final String TEST_PAGE_URL_FOO = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_TITLE_FOO = "Foo";
    private static final String TEST_FOLDER_TITLE = "Test folder";
    private static final String TEST_FOLDER_TITLE2 = "Test folder 2";
    private static final String TEST_TITLE_A = "a";
    private static final int TEST_PORT = 12345;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Use SigninTestRule to ensure accounts are always considered loaded, to ensure the sign-in
    // promo's visibility. See https://crbug.com/389733589.
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private SyncService mSyncService;
    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Captor private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    private final BookmarkManagerOpener mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();
    private BookmarkModel mBookmarkModel;
    // Constant but can only be initialized after parameterized test runner setup because this would
    // trigger native load / CommandLineFlag setup.
    private GURL mTestUrlA;
    private GURL mTestPage;
    private GURL mTestPageFoo;
    private EmbeddedTestServer mTestServer;

    // Page/Activity specific, set/updated when bookmarks UI is opened.
    private @Nullable BookmarkActivity mBookmarkActivity;
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private ModelList mModelList;
    private RecyclerView mItemsContainer;
    private BookmarkDelegate mDelegate;
    private DragReorderableRecyclerViewAdapter mAdapter;
    private BookmarkToolbar mToolbar;

    @Before
    public void setUp() {
        // Setup the shopping service.
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        mActivityTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                    SyncServiceFactory.setInstanceForTesting(mSyncService);
                });

        // Use a custom port so the links are consistent for render tests.
        mActivityTestRule
                .getActivityTestRule()
                .getEmbeddedTestServerRule()
                .setServerPort(TEST_PORT);
        mTestServer = mActivityTestRule.getTestServer();
        mTestUrlA = new GURL("http://a.com");
        mTestPage = new GURL(mTestServer.getURL(TEST_PAGE_URL_GOOGLE));
        mTestPageFoo = new GURL(mTestServer.getURL(TEST_PAGE_URL_FOO));
    }

    @After
    public void tearDown() throws Exception {
        if (mBookmarkActivity != null) ApplicationTestUtils.finishActivity(mBookmarkActivity);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky, crbug.com/342644856")
    public void testAddBookmark() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        // Check partner bookmarks are lazily loaded.
        assertFalse(mBookmarkModel.isBookmarkModelLoaded());

        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId id =
                            mBookmarkModel.getUserBookmarkIdForTab(
                                    mActivityTestRule.getActivity().getActivityTabProvider().get());
                    assertTrue(
                            "The test page is not added as bookmark: ",
                            mBookmarkModel.doesBookmarkExist(id));
                    BookmarkItem item = mBookmarkModel.getBookmarkById(id);
                    assertEquals(mBookmarkModel.getDefaultBookmarkFolder(), item.getParentId());
                    assertEquals(mTestPage, item.getUrl());
                    assertEquals(TEST_PAGE_TITLE_GOOGLE, item.getTitle());
                });

        BookmarkTestUtil.waitForOfflinePageSaved(mTestPage);

        // Click the star button again to launch the edit activity.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForEditActivity().finish();
    }

    @Test
    @SmallTest
    public void testAddBookmarkToOtherFolder() {
        mActivityTestRule.loadUrl(mTestPage);
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        // Set default folder as "Other Folder".
        runOnUiThreadBlocking(
                () -> {
                    ChromeSharedPreferences.getInstance()
                            .writeString(
                                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT,
                                    mBookmarkModel.getOtherFolderId().toString());
                });
        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId id =
                            mBookmarkModel.getUserBookmarkIdForTab(
                                    mActivityTestRule.getActivity().getActivityTabProvider().get());
                    assertTrue(
                            "The test page is not added as bookmark: ",
                            mBookmarkModel.doesBookmarkExist(id));
                    BookmarkItem item = mBookmarkModel.getBookmarkById(id);
                    assertEquals(
                            "Bookmark added in a wrong default folder.",
                            mBookmarkModel.getOtherFolderId(),
                            item.getParentId());
                });
    }

    @Test
    @SmallTest
    public void testOpenBookmark() throws InterruptedException, ExecutionException {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        assertTrue(
                "Grid view does not contain added bookmark: ",
                isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
        final View title = getViewWithText(mItemsContainer, TEST_PAGE_TITLE_GOOGLE);
        runOnUiThreadBlocking(() -> TouchCommon.singleClickView(title));
        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), is(mTestPage));
                });
    }

    @Test
    @SmallTest
    public void testUrlComposition() {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId mobileId = mBookmarkModel.getMobileFolderId();
                    BookmarkId bookmarkBarId = mBookmarkModel.getDesktopFolderId();
                    BookmarkId otherId = mBookmarkModel.getOtherFolderId();
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + mobileId,
                            BookmarkUiState.createFolderUrl(mobileId).toString());
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + bookmarkBarId,
                            BookmarkUiState.createFolderUrl(bookmarkBarId).toString());
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + otherId,
                            BookmarkUiState.createFolderUrl(otherId).toString());
                });
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testShowBookmarkManager_Phone() throws InterruptedException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        runOnUiThreadBlocking(
                () -> {
                    mBookmarkManagerOpener.showBookmarkManager(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule
                                    .getActivity()
                                    .getActivityTab(),
                            mActivityTestRule.getProfile(false),
                            mBookmarkModel.getMobileFolderId());
                });

        BookmarkTestUtil.waitForBookmarkActivity();

        // Assign so it's cleaned up after the test.
        mBookmarkActivity = (BookmarkActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "https://crbug.com/41484383")
    public void testOpenBookmarkManagerFolder() throws InterruptedException {
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkTestUtil.waitForBookmarkModelLoaded();

        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals("chrome-native://bookmarks/folder/3", mBookmarkManagerOpener.getLastUsedUrl());
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testFolderNavigation_Phone() throws InterruptedException, ExecutionException {
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Check that we are in the mobile bookmarks folder.
        assertEquals("Mobile bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.NORMAL_VIEW_BACK, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Open the new test folder.
        runOnUiThreadBlocking(() -> mDelegate.openFolder(testFolder));

        // Check that we are in the editable test folder.
        assertEquals(TEST_FOLDER_TITLE, mToolbar.getTitle());
        assertEquals(NavigationButton.NORMAL_VIEW_BACK, mToolbar.getNavigationButtonForTests());
        assertTrue(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(testFolder, TEST_FOLDER_TITLE2));

        // Check that the test folder reflects name changes.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mToolbar.getTitle(), equalTo(TEST_FOLDER_TITLE2)));

        // Call BookmarkToolbar#onClick() to activate the navigation button.
        runOnUiThreadBlocking(() -> mToolbar.onClick(mToolbar));

        // Check that we are back in the mobile folder
        assertEquals("Mobile bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.NORMAL_VIEW_BACK, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Call BookmarkToolbar#onClick() to activate the navigation button.
        runOnUiThreadBlocking(() -> mToolbar.onClick(mToolbar));

        // Check that we are in the root folder.
        assertEquals("Bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.NONE, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "https://crbug.com/41484383")
    @DisabledTest(message = "Proabably never worked. crbug.com/446200399")
    public void testEmptyBookmarkFolder() throws InterruptedException {
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        onView(withText("You'll find your bookmarks here")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "https://crbug.com/41484383")
    @DisabledTest(message = "Proabably never worked. crbug.com/446200399")
    public void testEmptyReadingListFolder() throws InterruptedException {
        openBookmarkManager();
        BookmarkTestUtil.openReadingList(mItemsContainer, mDelegate, mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        onView(withText("You'll find your reading list here")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @DisabledTest(message = "Proabably never worked. crbug.com/446200399")
    public void testEmptySearch() throws InterruptedException {
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        onView(withText("You'll find your bookmarks here")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "crbug.com/411305260")
    public void testOpenFromReadingListAndNavigateBack() throws Exception {
        openBookmarkManager();
        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addToReadingList(
                                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                                "test",
                                new GURL("https://test.com")));

        BookmarkTestUtil.openReadingList(mItemsContainer, mDelegate, mBookmarkModel);
        onView(withText("test")).perform(click());
        Espresso.pressBack();
        onView(withText("test")).check(matches(isDisplayed()));
    }

    // TODO(twellington): Write a folder navigation test for tablets that waits for the Tab hosting
    //                    the native page to update its url after navigations.

    @Test
    @MediumTest
    public void testSearchBookmarks() throws Exception {
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items before starting search.", 2, getBookmarkCount());

        final boolean isTablet =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity());
        if (isTablet) {
            onView(withId(R.id.row_search_text)).perform(replaceText("Google"));
        } else {
            enterSearch();
            assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
            assertEquals(
                    "No bookmarks should be shown when starting search.", 0, getBookmarkCount());
            searchBookmarks("Google");
        }
        assertEquals("Wrong number of items after searching.", 1, getBookmarkCount());

        BookmarkId newBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE2, mTestPage);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Wrong number of items after bookmark added while searching.",
                            getBookmarkCount(),
                            is(2));
                });

        removeBookmark(newBookmark);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Wrong number of items after bookmark removed while searching.",
                            getBookmarkCount(),
                            is(1));
                });

        if (isTablet) {
            onView(withId(R.id.row_search_text)).perform(replaceText("Non-existent page"));
        } else {
            searchBookmarks("Non-existent page");
        }
        assertEquals(
                "Wrong number of items after searching for non-existent item.",
                0,
                getBookmarkCount());

        if (isTablet) {
            onView(withId(R.id.row_search_text)).perform(replaceText(""));
        } else {
            exitSearch();
        }
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items after closing search UI.", 2, getBookmarkCount());
        assertEquals(TEST_FOLDER_TITLE, mToolbar.getTitle());
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testSearchBookmarks_pressBack() throws Exception {
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(
                true,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        runOnUiThreadBlocking(mDelegate::openSearchUi);

        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        assertEquals("No items are shown when a search is started.", 0, getBookmarkCount());

        assertEquals(
                true,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        exitSearch();

        // Enter search UI again.
        runOnUiThreadBlocking(mDelegate::openSearchUi);

        searchBookmarks("Google");
        assertEquals("Wrong number of items after searching.", 1, getBookmarkCount());

        ImprovedBookmarkRow itemView = getNthBookmarkRow(1);
        startSelectionThroughMoreMenu(itemView);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isSelectedForTesting, "Expected item \"test\" to become selected");

        pressBackButton();

        // Clear selection but still in search UI.
        CriteriaHelper.pollUiThread(
                () -> !itemView.isSelectedForTesting(),
                "Expected item \"test\" to become not selected");
        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        assertEquals(
                true,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        // Exit search UI.
        exitSearch();
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());

        // Exit folder.
        assertEquals(
                true,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
        pressBackButton();
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());

        // Exit bookmark activity.
        assertEquals(
                false,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
        runOnUiThreadBlocking(mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
    @Restriction({DeviceFormFactor.PHONE})
    public void testSearchBookmarks_pressEscape() throws Exception {
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        openBookmarkManager();
        openFolder(folder);

        // Enter search and select an item
        runOnUiThreadBlocking(mDelegate::openSearchUi);
        searchBookmarks("Google");
        ImprovedBookmarkRow itemView = getNthBookmarkRow(1);
        startSelectionThroughMoreMenu(itemView);
        CriteriaHelper.pollUiThread(
                itemView::isSelectedForTesting, "Expected item to become selected");
        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());

        // Pressing escape should clear selection but remain in search
        pressEscapeKey();
        CriteriaHelper.pollUiThread(
                () -> !itemView.isSelectedForTesting(), "Expected item to become unselected");
        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());

        // Pressing escape again should exit search
        pressEscapeKey();
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());

        // After exiting search, we are in a subfolder. Pressing Escape again
        // should navigate up the folder hierarchy, just like the back button.
        assertEquals(
                "Handler should be active to navigate up from the folder.",
                true,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
        pressEscapeKey();
        assertEquals(
                "After third escape press, should be in the root folder.",
                BookmarkUiMode.FOLDER,
                mDelegate.getCurrentUiMode());

        // At the root level, there are no more states to pop. The handler becomes inactive, and
        // pressing Escape again should do nothing.
        assertEquals(
                "Handler should be inactive at the root level.",
                false,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
    @Restriction({DeviceFormFactor.ONLY_TABLET})
    public void testTabletSearch_EscapeKeyClearsSearch() throws Exception {
        // Setup: Add a bookmark so the folder isn't empty.
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        openBookmarkManager();
        openFolder(folder);

        // Verify initial state.
        assertEquals(
                "Should be in folder view.", BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        onView(withId(R.id.row_search_text)).check(matches(withText("")));

        // Action 1: User types in the search bar.
        onView(withId(R.id.row_search_text)).perform(replaceText("Google"));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // Verification 1: Search results are shown.
        assertEquals("Search should find one item.", 1, getBookmarkCount());
        onView(withText(TEST_PAGE_TITLE_GOOGLE)).check(matches(isDisplayed()));

        // Action 2: User presses the Escape key.
        pressEscapeKey();

        // Verification 2: The search text is cleared, and the folder contents are restored.
        onView(withId(R.id.row_search_text)).check(matches(withText("")));
        assertEquals("Clearing search should show all items in the folder.", 1, getBookmarkCount());
        assertEquals(
                "Should still be in folder mode.",
                BookmarkUiMode.FOLDER,
                mDelegate.getCurrentUiMode());

        // Action 3: User presses Escape again.
        // The handler should now be disabled, so we expect it to do nothing (return false).
        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "handleEscPress should return false when search is empty.",
                            false,
                            mBookmarkManagerCoordinator.handleEscPress());
                });
    }

    @Test
    @MediumTest
    public void testSearchBookmarks_Delete() throws Exception {
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, testFolder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(testFolder);

        assertEquals(
                "Wrong state, should be in folder",
                BookmarkUiMode.FOLDER,
                mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items before starting search.", 1, getBookmarkCount());

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())) {
            onView(withId(R.id.row_search_text)).perform(replaceText(TEST_PAGE_TITLE_GOOGLE));
        } else {
            enterSearch();
            assertEquals(
                    "Wrong state, should be searching",
                    BookmarkUiMode.SEARCHING,
                    mDelegate.getCurrentUiMode());
            assertEquals("Wrong number after starting search.", 0, getBookmarkCount());
            searchBookmarks(TEST_PAGE_TITLE_GOOGLE);
        }
        assertEquals("Wrong number item items when searching.", 1, getBookmarkCount());

        // Select the bookmark and delete it.
        ImprovedBookmarkRow row = getNthBookmarkRow(1);
        startSelectionThroughLongPress(row);
        clickToolbarMenuItem(R.id.selection_mode_delete_menu_id);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // The user should still be searching, and the bookmark should be gone.
        pollForModeAndCount(
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())
                        ? BookmarkUiMode.FOLDER
                        : BookmarkUiMode.SEARCHING,
                0);

        // // Undo the deletion.
        runOnUiThreadBlocking(
                () -> mBookmarkManagerCoordinator.getUndoControllerForTesting().onAction(null));

        // The user should still be searching, and the bookmark should reappear.
        pollForModeAndCount(
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())
                        ? BookmarkUiMode.FOLDER
                        : BookmarkUiMode.SEARCHING,
                1);
    }

    @Test
    @MediumTest
    public void testSearchBookmarks_DeleteFolderWithChildrenInResults() throws Exception {
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder);
        openBookmarkManager();

        // Start searching, enter a query.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())) {
            onView(withId(R.id.row_search_text)).perform(replaceText("test"));
        } else {
            runOnUiThreadBlocking(mDelegate::openSearchUi);
            assertEquals(
                    "Wrong state, should be searching",
                    BookmarkUiMode.SEARCHING,
                    mDelegate.getCurrentUiMode());
            searchBookmarks("test");
        }
        assertEquals("Wrong number of items after searching.", 2, getBookmarkCount());

        // Remove the bookmark.
        removeBookmark(testFolder);

        // The user should still be searching, and the bookmark should be gone.
        pollForModeAndCount(
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())
                        ? BookmarkUiMode.FOLDER
                        : BookmarkUiMode.SEARCHING,
                0);

        // Undo the deletion.
        runOnUiThreadBlocking(
                () -> mBookmarkManagerCoordinator.getUndoControllerForTesting().onAction(null));

        // The user should still be searching, and the bookmark should reappear.
        pollForModeAndCount(
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())
                        ? BookmarkUiMode.FOLDER
                        : BookmarkUiMode.SEARCHING,
                2);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE}) // Tablets don't have a close button.
    public void testCloseBookmarksWhileStillLoading() throws Exception {
        BookmarkManagerCoordinator.preventLoadingForTesting(true);

        openBookmarkManager();

        clickToolbarMenuItem(R.id.close_menu_id);

        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);

        BookmarkManagerCoordinator.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE}) // see crbug.com/1429025
    public void testEditHiddenWhileStillLoading() throws Exception {
        BookmarkManagerCoordinator.preventLoadingForTesting(true);

        openBookmarkManager();

        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        BookmarkManagerCoordinator.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testStopSpinnerOnEmptyFolder() throws Exception {
        // Cannot have a promo if we're going to have 0 elements in RecyclerView.
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);

        // Force BookmarkModel to be loaded so we can get a folder id later.
        loadBookmarkModel();

        // This will cause opening the bookmarks UI to load the mobile folder.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId folderId = mBookmarkModel.getMobileFolderId();
                    String prefUrl = BookmarkUiState.createFolderUrl(folderId).toString();
                    BookmarkUtils.setLastUsedUrl(prefUrl);
                });

        // Prevent loading so we can verify we see the spinner initially.
        BookmarkManagerCoordinator.preventLoadingForTesting(true);
        openBookmarkManager();

        // Loading view a child of the SelectableListLayout, not the RecyclerView.
        View parent = (View) mItemsContainer.getParent();
        View loadingView = parent.findViewById(R.id.loading_view);

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(loadingView.getVisibility(), is(View.VISIBLE)));

        // The idea is that the manager should now be able to figure out what rows it can populate.
        // However if there are no rows created, because we have an empty folder, no events
        // naturally reach the SelectableListLayout's observer. So the manager will have to manually
        // notify.
        BookmarkManagerCoordinator.preventLoadingForTesting(false);
        runOnUiThreadBlocking(mBookmarkManagerCoordinator::finishLoadingForTesting);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(loadingView.getVisibility(), is(View.GONE));
                });
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSelectionMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_TITLE_A, mTestUrlA);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow a = getNthBookmarkRow(1);
        View aMoreButton = a.findViewById(R.id.more);

        ImprovedBookmarkRow folder = getNthBookmarkRow(2);
        View folderMoreButton = folder.findViewById(R.id.more);

        startSelectionThroughMoreMenu(folder);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(
                folder::isSelectedForTesting, "Expected item \"test\" to become selected");

        assertEquals(
                "Expected bookmark toolbar to be selection mode",
                ViewType.SELECTION_VIEW,
                mToolbar.getCurrentViewType());
        assertEquals(
                "Expected more button of selected item to be gone when drag is active.",
                View.GONE,
                folderMoreButton.getVisibility());

        assertEquals(
                "Expected more button of unselected item to be gone when drag is active.",
                View.VISIBLE,
                aMoreButton.getVisibility());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://issues.chromium.org/331232180")
    public void testEndIconVisibilityInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_TITLE_A);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow test = getNthBookmarkRow(2);
        View testMoreButton = test.findViewById(R.id.more);

        ImprovedBookmarkRow a = getNthBookmarkRow(1);
        View aMoreButton = a.findViewById(R.id.more);

        enterSearch();
        searchBookmarks(TEST_TITLE_A);

        // When searching, the promo is removed. Index 1 is now `test`.
        startSelectionThroughMoreMenu(test);
        CriteriaHelper.pollUiThread(
                test::isSelectedForTesting, "Expected item \"test\" to become selected");

        assertEquals(
                "Expected more button of selected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE,
                testMoreButton.getVisibility());

        assertEquals(
                "Expected more button of unselected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE,
                aMoreButton.getVisibility());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/344981899, this test needs to be fixed post-UNO")
    public void testSmallDrag_Up_BookmarksOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        BookmarkId aId = addBookmark(TEST_TITLE_A, mTestUrlA);

        // When bookmarks are added, they are added to the top of the list.
        // The current bookmark order is the reverse of the order in which they were added.
        initial.add(aId);
        initial.add(googleId);
        initial.add(fooId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultBookmarkFolder())
                                    .subList(0, 3));
                });

        expected.add(fooId);
        expected.add(aId);
        expected.add(googleId);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        ImprovedBookmarkRow foo = getNthBookmarkRow(3);
        assertEquals(
                "Wrong bookmark item selected.", TEST_PAGE_TITLE_FOO, foo.getTitleForTesting());
        startSelectionThroughMoreMenu(foo);

        // Starts as last bookmark (2nd index) and ends as 0th bookmark (promo header not included).
        simulateDragForTestsOnUiThread(getNthBookmarkIndex(3), getNthBookmarkIndex(1));

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultBookmarkFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 3));
                    assertTrue(
                            "The selected item should stay selected", foo.isSelectedForTesting());
                });

        // After a drag is finished, the toolbar menu items should still reflect the selected state.
        // Check inspired by https://crbug.com/1434566.
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_open_in_new_tab_id).isVisible());
        assertTrue(
                mToolbar.getMenu().findItem(R.id.selection_open_in_incognito_tab_id).isVisible());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/344981899, this test needs to be fixed post-UNO")
    public void testSmallDrag_Down_FoldersOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addFolder("b");
        BookmarkId cId = addFolder("c");
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(cId);
        initial.add(bId);
        initial.add(aId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultBookmarkFolder())
                                    .subList(0, 4));
                });

        expected.add(cId);
        expected.add(bId);
        expected.add(aId);
        expected.add(testId);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        ImprovedBookmarkRow test = getNthBookmarkRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitleForTesting());

        startSelectionThroughMoreMenu(test);

        // Starts as 0th bookmark (not counting promo header) and ends as last (index 3).
        simulateDragForTestsOnUiThread(getNthBookmarkIndex(1), getNthBookmarkIndex(4));

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultBookmarkFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 4));
                    assertTrue(
                            "The selected item should stay selected", test.isSelectedForTesting());
                });
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_MixedFoldersAndBookmarks() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addBookmark("b", new GURL("http://b.com"));
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(bId);
        initial.add(aId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultBookmarkFolder())
                                    .subList(0, 3));
                });

        expected.add(bId);
        expected.add(testId);
        expected.add(aId);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };
        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        ImprovedBookmarkRow test = getNthBookmarkRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitleForTesting());

        startSelectionThroughMoreMenu(test);

        // Starts as 0th bookmark (not counting promo header) and ends at the 1st index.
        simulateDragForTestsOnUiThread(getNthBookmarkIndex(1), getNthBookmarkIndex(2));

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultBookmarkFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 3));
                    assertTrue(
                            "The selected item should stay selected", test.isSelectedForTesting());
                });
    }

    @Test
    @MediumTest
    public void testPromoDraggability() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Ensure that the sign-in promo is visible before testing its draggability.
        onViewWaiting(withId(R.id.signin_promo_view_container));
        ViewHolder promo = getViewHolderAtIndex(1);
        ImprovedBookmarkRow row = getNthBookmarkRow(1);
        startSelectionThroughMoreMenu(row);

        assertFalse(
                "Promo header should not be passively draggable",
                isViewHolderPassivelyDraggable(promo));
        assertFalse(
                "Promo header should not be actively draggable",
                isViewHoldersActivelyDraggable(promo));
    }

    @Test
    @MediumTest
    public void testItemDraggability() throws Exception {
        addBookmark("a", mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ViewHolder viewHolder = getNthBookmarkViewHolder(1);
        assertTrue(
                "Unselected rows should be passively draggable",
                isViewHolderPassivelyDraggable(viewHolder));
        assertTrue(
                "Unselected rows should not be actively draggable",
                isViewHoldersActivelyDraggable(viewHolder));
    }

    @Test
    @MediumTest
    public void testCannotSelectPromo() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();

        View promo = getNthBookmarkViewHolder(1).itemView;
        TouchCommon.longPressView(promo);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertFalse(
                "Expected that we would not be in selection mode "
                        + "after long pressing on promo view.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "https://crbug.com/41484383")
    public void testMoveUpMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);

        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        View google = getNthBookmarkViewHolder(2).itemView;
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_PAGE_TITLE_GOOGLE,
                ((ImprovedBookmarkRow) google).getTitleForTesting());
        View more = google.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        assertTrue(getNthBookmarkRow(1).getTitleForTesting().equals(TEST_PAGE_TITLE_GOOGLE));
        assertTrue(getNthBookmarkRow(2).getTitleForTesting().equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "https://crbug.com/41484383")
    public void testMoveDownMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow testFolder = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                testFolder.getTitleForTesting());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        assertTrue(getNthBookmarkRow(1).getTitleForTesting().equals(TEST_PAGE_TITLE_GOOGLE));
        assertTrue(getNthBookmarkRow(2).getTitleForTesting().equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownGoneForBottomElement() throws Exception {
        addBookmarkWithPartner(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow google = getNthBookmarkRow(2);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_PAGE_TITLE_GOOGLE,
                google.getTitleForTesting());
        View more = google.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveUpGoneForTopElement() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow testFolder = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                testFolder.getTitleForTesting());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1046653")
    public void testMoveButtonsGoneInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        enterSearch();
        ImprovedBookmarkRow testFolder = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                testFolder.getTitleForTesting());
        View more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneWithOneBookmark() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ImprovedBookmarkRow testFolder = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                testFolder.getTitleForTesting());
        View more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testTopLevelFolders() throws Exception {
        // NOTE: Hide promos to ensure top level-folders will fit the viewport.
        SigninPromoCoordinator.disablePromoForTesting();
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();
        onViewWaiting(allOf(withText("Mobile bookmarks"), isDisplayed()));
        onViewWaiting(allOf(withText("Reading list"), isDisplayed()));
        onView(withText("Bookmarks bar"))
                .check(
                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(
                                        mActivityTestRule.getActivity())
                                ? matches(isDisplayed())
                                : doesNotExist());
    }

    @Test
    @MediumTest
    public void testTopLevelFolderUpdateAfterSync() throws Exception {
        // Set up the test and open the bookmark manager to the Mobile Bookmarks folder.
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        openBookmarkManager();

        // Add a bookmark to the Other Bookmarks folder.
        runOnUiThreadBlocking(
                () -> {
                    assertNotNull(
                            mBookmarkModel.addBookmark(
                                    mBookmarkModel.getOtherFolderId(), 0, TEST_TITLE_A, mTestUrlA));
                });

        verify(mSyncService, atLeast(1))
                .addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        for (SyncStateChangedListener syncStateChangedListener :
                mSyncStateChangedListenerCaptor.getAllValues()) {
            runOnUiThreadBlocking(syncStateChangedListener::syncStateChanged);
        }
        runOnUiThreadBlocking(getTestingDelegate()::simulateSignInForTesting);

        final List<String> expectedTopLevelFolders =
                new ArrayList<>(List.of("Mobile bookmarks", "Other bookmarks", "Reading list"));

        if (BookmarkBarUtils.isDeviceBookmarkBarCompatible(mActivityTestRule.getActivity())) {
            expectedTopLevelFolders.add(1, "Bookmarks bar");
        }

        final String expectedTopLevelFoldersStr =
                IntStream.range(0, expectedTopLevelFolders.size())
                        .mapToObj(
                                i -> {
                                    final var folder = expectedTopLevelFolders.get(i);
                                    final var isLast = i == expectedTopLevelFolders.size() - 1;
                                    final var conjunction = isLast ? "and " : "";
                                    return String.format("%s\"%s\"", conjunction, folder);
                                })
                        .collect(Collectors.joining(", "));

        assertEquals(
                String.format("Expected promo, %s folder to appear!", expectedTopLevelFoldersStr),
                expectedTopLevelFolders.size(),
                getBookmarkCount());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky, see crbug.com/335891831")
    public void testShowInFolder_NoScroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Enter search mode.
        enterSearch();
        searchBookmarks(TEST_FOLDER_TITLE);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // Click "Show in folder".
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(scrollTo(), click());

        CriteriaHelper.pollUiThread(
                () -> mModelList.get(1).model.get(BookmarkManagerProperties.IS_HIGHLIGHTED));

        // Enter search mode again.
        enterSearch();
        searchBookmarks(TEST_FOLDER_TITLE);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        CriteriaHelper.pollUiThread(
                () -> !mModelList.get(1).model.get(BookmarkManagerProperties.IS_HIGHLIGHTED));

        // Click "Show in folder" again.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(scrollTo(), click());

        CriteriaHelper.pollUiThread(
                () -> mModelList.get(1).model.get(BookmarkManagerProperties.IS_HIGHLIGHTED));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky, see crbug.com/335891831")
    public void testShowInFolder_Scroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE); // Index 8
        addBookmark(TEST_TITLE_A, mTestUrlA);
        addBookmark(TEST_PAGE_TITLE_FOO, new GURL("http://foo.com"));
        addFolder(TEST_PAGE_TITLE_GOOGLE2);
        addFolder("B");
        addFolder("C");
        addFolder("D");
        addFolder("E"); // Index 1
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();
        searchBookmarks(TEST_FOLDER_TITLE);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);

        // Show in folder.
        onView(withText("Show in folder")).perform(scrollTo(), click());

        // This should be in the 8th position now.
        ImprovedBookmarkRow testFolderInList = getNthBookmarkRow(8);
        assertFalse("Expected list to scroll bookmark item into view", testFolderInList == null);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                ((ImprovedBookmarkRow) testFolderInList).getTitleForTesting());
        assertTrue(
                "Expected highlight to pulse on after scrolling to the item!",
                checkHighlightPulse(testFolderInList));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://issues.chromium.org/331232180")
    public void testShowInFolder_OpenOtherFolder() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        runOnUiThreadBlocking(() -> mBookmarkModel.addBookmark(testId, 0, TEST_TITLE_A, mTestUrlA));
        BookmarkPromoHeader.forcePromoVisibilityForTesting(true);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();
        searchBookmarks(mTestUrlA.getSpec());

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_TITLE_A);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // Make sure that we're in the right folder (index 1 because of promo).
        ImprovedBookmarkRow itemA = getNthBookmarkRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_TITLE_A, itemA.getTitleForTesting());

        assertTrue(
                "Expected highlight to pulse after opening an item in another folder!",
                checkHighlightPulse(itemA));

        // Open mobile bookmarks folder, then go back to the subfolder.
        BookmarkId mobileFolderId =
                runOnUiThreadBlocking(
                        () -> {
                            return mBookmarkModel.getMobileFolderId();
                        });
        openFolder(mobileFolderId);
        openFolder(testId);

        ImprovedBookmarkRow itemASecondView = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_TITLE_A,
                itemASecondView.getTitleForTesting());
        assertTrue(
                "Expected highlight to not be highlighted after exiting and re-entering folder!",
                checkHighlightOff(itemASecondView));
    }

    @Test
    @SmallTest
    public void testAddBookmarkInBackgroundWithSelection() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(1, getBookmarkCount());
        ImprovedBookmarkRow row = getNthBookmarkRow(1);
        startSelectionThroughMoreMenu(row);

        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addBookmark(folder, 1, TEST_PAGE_TITLE_GOOGLE, mTestPage);
                });

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        runOnUiThreadBlocking(
                () -> {
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, getBookmarkCount());
                    assertTrue(
                            "The selected row should be kept selected",
                            getNthBookmarkRow(1).isSelectedForTesting());
                });
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://issues.chromium.org/331232180")
    public void testDeleteAllSelectedBookmarksInBackground() throws Exception {
        // Select one bookmark and then remove that in background.
        // In the meantime, the toolbar changes from selection mode to normal mode.
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(3, getBookmarkCount());
        ImprovedBookmarkRow row = getNthBookmarkRow(2);
        startSelectionThroughMoreMenu(row);
        CallbackHelper helper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mDelegate.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
                });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        helper.waitForOnly();
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Item is not deleted",
                            isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, getBookmarkCount());
                    assertEquals(
                            "Bookmark View should be back to normal view",
                            ViewType.NORMAL_VIEW,
                            mToolbar.getCurrentViewType());
                });
    }

    @Test
    @SmallTest
    public void testDeleteSomeSelectedBookmarksInBackground() throws Exception {
        // selected on bookmarks and then remove one of them in background
        // in the meantime, the toolbar stays in selection mode
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(3, getBookmarkCount());
        startSelectionThroughLongPress(getNthBookmarkRow(1));
        toggleSelectionThroughClick(getNthBookmarkRow(2));
        CallbackHelper helper = new CallbackHelper();

        runOnUiThreadBlocking(
                () -> {
                    mDelegate.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
                });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        helper.waitForNext();
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Item is not deleted",
                            isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, getBookmarkCount());
                    assertTrue(
                            "Item selected should not be cleared",
                            getNthBookmarkRow(1).isSelectedForTesting());
                    assertEquals(
                            "Should stay in selection mode because there is one selected",
                            ViewType.SELECTION_VIEW,
                            mToolbar.getCurrentViewType());
                });
    }

    @Test
    @SmallTest
    public void testUpdateSelectedBookmarkInBackground() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(1, getBookmarkCount());
        startSelectionThroughMoreMenu(getNthBookmarkRow(1));
        CallbackHelper helper = new CallbackHelper();
        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addObserver(
                                new BookmarkModelObserver() {
                                    @Override
                                    public void bookmarkModelChanged() {
                                        helper.notifyCalled();
                                    }
                                }));

        runOnUiThreadBlocking(() -> mBookmarkModel.setBookmarkTitle(id, TEST_PAGE_TITLE_GOOGLE));

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(1, getBookmarkCount());
                    assertTrue(
                            "The selected row should stay selected",
                            getNthBookmarkRow(1).isSelectedForTesting());
                });
    }

    @Test
    @MediumTest
    public void testBookmarksDoesNotRecordLaunchMetrics() throws Throwable {
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();

        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        onView(withText(TEST_PAGE_TITLE_GOOGLE)).perform(click());
        BookmarkTestUtil.waitForTabbedActivity();
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));
    }

    /**
     * Test that we record Bookmarks.BookmarkTestUtil.openBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in regular mode.
     *
     * <p>Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via BookmarkTestUtil.openBookmarkManager test method which circumvents the
     * click of R.id.all_bookmarks_menu_id, this doesn't happen in actual case and the metric indeed
     * gets recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testRecordsHistogramWhenBookmarkManagerOpened_InRegular() throws Throwable {
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.REGULAR));
    }

    /**
     * Test that we record Bookmarks.OpenBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in Incognito mode.
     *
     * <p>Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via openBookmarkManager test method which circumvents the click of
     * R.id.all_bookmarks_menu_id. This doesn't happen in actual case and the metric indeed gets
     * recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(message = "https://crbug.com/344981899, this test needs to be fixed post-UNO")
    public void testRecordsHistogramWhenBookmarkManagerOpened_InIncognito() throws Throwable {
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.INCOGNITO));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testShoppingFilterInBookmarks() throws InterruptedException, ExecutionException {
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        PowerBookmarkMeta.Builder meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
                });
        doReturn(true)
                .when(mShoppingService)
                .isSubscribedFromCache(
                        PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                meta.build().getShoppingSpecifics()));
        doCallback((Callback<List<BookmarkId>> callback) -> callback.onResult(Arrays.asList(id)))
                .when(mShoppingService)
                .getAllPriceTrackedBookmarks(any());

        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        runOnUiThreadBlocking(
                () -> {
                    mDelegate.openFolder(mBookmarkModel.getRootFolderId());
                });

        onView(withText("Tracked products")).perform(click());

        // Check that we are in the mobile bookmarks folder.
        assertEquals("Bookmarks", mToolbar.getTitle());
        assertEquals(
                "Shopping bookmark is present.",
                TEST_PAGE_TITLE_GOOGLE,
                getNthBookmarkRow(1).getTitleForTesting());
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testShoppingDataPresentButFeatureDisabled()
            throws InterruptedException, ExecutionException {
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        PowerBookmarkMeta.Builder meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
                });
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        onView(withText("Tracked products")).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testEdgeToEdge() throws InterruptedException {
        openBookmarkManager();
        RecyclerView recyclerView = mBookmarkManagerCoordinator.getRecyclerViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ObservableSupplier<EdgeToEdgeController> supplier =
                            mBookmarkActivity.getEdgeToEdgeSupplier();
                    EdgeToEdgeController edgeToEdgeController =
                            supplier == null ? null : supplier.get();
                    int bottomInset =
                            edgeToEdgeController != null && edgeToEdgeController.isDrawingToEdge()
                                    ? edgeToEdgeController.getBottomInsetPx()
                                    : 0;
                    assertEquals(bottomInset, recyclerView.getPaddingBottom());
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testEdgeToEdge_editView() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Click the edit button for the bookmark.
        ImprovedBookmarkRow row = getNthBookmarkRow(1);
        View more = row.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Edit")).perform(click());

        // Get the BookmarkEditActivity.
        final BookmarkEditActivity editActivity =
                (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Check the padding.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ScrollView scrollView = editActivity.getScrollViewForTesting();
                    ObservableSupplier<EdgeToEdgeController> supplier =
                            editActivity.getEdgeToEdgeSupplier();
                    EdgeToEdgeController edgeToEdgeController =
                            supplier == null ? null : supplier.get();
                    int bottomInset =
                            edgeToEdgeController != null && edgeToEdgeController.isDrawingToEdge()
                                    ? edgeToEdgeController.getBottomInsetPx()
                                    : 0;
                    assertEquals(bottomInset, scrollView.getPaddingBottom());
                });

        // Need to manually finish the edit activity to fully clean up the test.
        editActivity.finish();
    }

    private void openBookmarkManager() throws InterruptedException {
        if (mActivityTestRule.getActivity().isTablet()) {
            String rootFolderId = "folder/0";
            mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_NATIVE_URL + rootFolderId);
            mItemsContainer =
                    mActivityTestRule
                            .getActivity()
                            .findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mBookmarkManagerCoordinator =
                    ((BookmarkPage) mActivityTestRule.getActivityTab().getNativePage())
                            .getManagerForTesting();
        } else {
            // Phone.
            mBookmarkActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BookmarkActivity.class,
                            new MenuUtils.MenuActivityTrigger(
                                    InstrumentationRegistry.getInstrumentation(),
                                    mActivityTestRule.getActivity(),
                                    R.id.all_bookmarks_menu_id));
            ViewUtils.waitForView(
                    (ViewGroup) mBookmarkActivity.getWindow().getDecorView().getRootView(),
                    ViewMatchers.withId(R.id.selectable_list));
            mItemsContainer = mBookmarkActivity.findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mBookmarkManagerCoordinator = mBookmarkActivity.getManagerForTesting();
        }

        mModelList = mBookmarkManagerCoordinator.getModelListForTesting();
        mDelegate = mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
        mAdapter = (DragReorderableRecyclerViewAdapter) mItemsContainer.getAdapter();
        mToolbar = mBookmarkManagerCoordinator.getToolbarForTesting();

        runOnUiThreadBlocking(
                () -> AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false));
    }

    private boolean isItemPresentInBookmarkList(final String expectedTitle) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public Boolean call() {
                        for (int i = 0; i < getBookmarkCount(); i++) {
                            BookmarkId id =
                                    getTestingDelegate().getBookmarkIdByPositionForTesting(i);

                            if (id == null) continue;

                            String actualTitle = mBookmarkModel.getBookmarkTitle(id);
                            if (TextUtils.equals(actualTitle, expectedTitle)) {
                                return true;
                            }
                        }
                        return false;
                    }
                });
    }

    /**
     * Adds a bookmark in the scenario where we have partner bookmarks.
     *
     * @param title The title of the bookmark to add.
     * @param url The url of the bookmark to add.
     * @return The BookmarkId of the added bookmark.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addBookmarkWithPartner(String title, GURL url) throws ExecutionException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        return runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addBookmark(
                                mBookmarkModel.getDefaultBookmarkFolder(), 0, title, url));
    }

    /**
     * Adds a folder in the scenario where we have partner bookmarks.
     *
     * @param title The title of the folder to add.
     * @return The BookmarkId of the added folder.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addFolderWithPartner(String title) throws ExecutionException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        return runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addFolder(
                                mBookmarkModel.getDefaultBookmarkFolder(), 0, title));
    }

    private void simulateDragForTestsOnUiThread(int start, int end) {
        runOnUiThreadBlocking(() -> mAdapter.simulateDragForTests(start, end));
    }

    private boolean isViewHolderPassivelyDraggable(ViewHolder viewHolder) {
        return ThreadUtils.runOnUiThreadBlocking(() -> mAdapter.isPassivelyDraggable(viewHolder));
    }

    private boolean isViewHoldersActivelyDraggable(ViewHolder viewHolder) {
        return ThreadUtils.runOnUiThreadBlocking(() -> mAdapter.isActivelyDraggable(viewHolder));
    }

    private BookmarkManagerTestingDelegate getTestingDelegate() {
        return mBookmarkManagerCoordinator.getTestingDelegate();
    }

    private void enterSearch() throws Exception {
        onView(withId(R.id.row_search_text)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    return mDelegate.getCurrentUiMode() == BookmarkUiMode.SEARCHING;
                },
                "Expected to enter search mode");
    }

    private void exitSearch() throws Exception {
        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        pressBackButton();

        CriteriaHelper.pollUiThread(
                () -> {
                    return mDelegate.getCurrentUiMode() == BookmarkUiMode.FOLDER;
                },
                "Expected to enter search mode");
    }

    private void clickMoreButtonOnFirstItem(String expectedBookmarkItemTitle) throws Exception {
        ImprovedBookmarkRow firstItem = getNthBookmarkRow(1);
        assertEquals(
                "Wrong bookmark item selected.",
                expectedBookmarkItemTitle,
                firstItem.getTitleForTesting());
        View more = firstItem.findViewById(R.id.more);
        runOnUiThreadBlocking(more::performClick);
        Thread.sleep(100);
    }

    /**
     * Returns the View that has the given text.
     *
     * @param viewGroup The group to which the view belongs.
     * @param expectedText The expected description text.
     * @return The unique view, if one exists. Throws an exception if one doesn't exist.
     */
    private static View getViewWithText(final ViewGroup viewGroup, final String expectedText) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public View call() {
                        ArrayList<View> outViews = new ArrayList<>();
                        ArrayList<View> matchingViews = new ArrayList<>();
                        viewGroup.findViewsWithText(
                                outViews, expectedText, View.FIND_VIEWS_WITH_TEXT);
                        // outViews includes all views whose text contains expectedText as a
                        // case-insensitive substring. Filter these views to find only exact string
                        // matches.
                        for (View v : outViews) {
                            if (TextUtils.equals(
                                    ((TextView) v).getText().toString(), expectedText)) {
                                matchingViews.add(v);
                            }
                        }
                        assertEquals(
                                "Exactly one item should be present.", 1, matchingViews.size());
                        return matchingViews.get(0);
                    }
                });
    }

    private void startSelectionThroughLongPress(ImprovedBookmarkRow improvedBookmarkRow) {
        assertFalse(
                "Selection can only be started with a long click, use #toggleSelectionThroughClick"
                        + " instead.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
        boolean wasInitiallySelected = improvedBookmarkRow.isSelectedForTesting();
        toggleSelectionAndEndAnimation(
                improvedBookmarkRow,
                () -> {
                    runOnUiThreadBlocking(
                            () -> {
                                improvedBookmarkRow.performLongClick();
                            });
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertNotEquals(wasInitiallySelected, improvedBookmarkRow.isSelectedForTesting());
    }

    private void toggleSelectionThroughClick(ImprovedBookmarkRow improvedBookmarkRow) {
        assertTrue(
                "Selection mode must already be enabled to select more.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
        boolean wasInitiallySelected = improvedBookmarkRow.isSelectedForTesting();
        toggleSelectionAndEndAnimation(
                improvedBookmarkRow,
                () -> {
                    runOnUiThreadBlocking(
                            () -> {
                                improvedBookmarkRow.performClick();
                            });
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertNotEquals(wasInitiallySelected, improvedBookmarkRow.isSelectedForTesting());
    }

    private void startSelectionThroughMoreMenu(ImprovedBookmarkRow improvedBookmarkRow) {
        assertFalse(
                "Selection can only be started through the more menu, use"
                        + " #toggleSelectionThroughClick instead.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
        toggleSelectionAndEndAnimation(
                improvedBookmarkRow,
                () -> {
                    View moreButton = improvedBookmarkRow.findViewById(R.id.more);
                    assertEquals(View.VISIBLE, moreButton.getVisibility());
                    runOnUiThreadBlocking(moreButton::callOnClick);

                    // Doesn't have a stable id to look up with. Use resolved text instead.
                    String selectText =
                            improvedBookmarkRow
                                    .getResources()
                                    .getString(R.string.bookmark_item_select);
                    onView(withText(selectText)).perform(click());
                });
    }

    private void toggleSelectionAndEndAnimation(
            ImprovedBookmarkRow improvedBookmarkRow, Runnable toggleRowImpl) {
        boolean wasInitiallySelected = improvedBookmarkRow.isSelectedForTesting();
        toggleRowImpl.run();
        runOnUiThreadBlocking(
                () -> {
                    // TODO: Is this even necessary?
                    // improvedBookmarkRow.endAnimationsForTests();
                    mToolbar.endAnimationsForTesting();
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertNotEquals(wasInitiallySelected, improvedBookmarkRow.isSelectedForTesting());
    }

    private BookmarkId addBookmark(final String title, GURL url, BookmarkId parent)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        return runOnUiThreadBlocking(() -> mBookmarkModel.addBookmark(parent, 0, title, url));
    }

    private BookmarkId addBookmark(final String title, final GURL url) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        return runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addBookmark(
                                mBookmarkModel.getDefaultBookmarkFolder(), 0, title, url));
    }

    private BookmarkId addFolder(final String title) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        return runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addFolder(
                                mBookmarkModel.getDefaultBookmarkFolder(), 0, title));
    }

    private void removeBookmark(final BookmarkId bookmarkId) {
        runOnUiThreadBlocking(() -> mBookmarkModel.deleteBookmark(bookmarkId));
    }

    private void searchBookmarks(final String query) {
        runOnUiThreadBlocking(() -> getTestingDelegate().searchForTesting(query));
        // If the RecyclerView is GONE, it will never perform layout, and never stabilize.
        if (mItemsContainer.getVisibility() == View.VISIBLE) {
            RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        }
    }

    private void openFolder(BookmarkId folder) {
        runOnUiThreadBlocking(() -> mDelegate.openFolder(folder));
        // If the RecyclerView is GONE, it will never perform layout, and never stabilize.
        if (mItemsContainer.getVisibility() == View.VISIBLE) {
            RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        }
    }

    private void pressBackButton() throws Exception {
        runOnUiThreadBlocking(mBookmarkManagerCoordinator::handleBackPress);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
    }

    private void clickToolbarMenuItem(@IdRes int menuId) throws ExecutionException {
        runOnUiThreadBlocking(() -> mToolbar.onMenuItemClick(mToolbar.getMenu().findItem(menuId)));
    }

    private int getNthBookmarkIndex(int n) {
        int index = 0;
        for (; index < mModelList.size(); index++) {
            ListItem item = mModelList.get(index);
            if (item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_VISUAL
                    || item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_COMPACT) {
                n--;
                if (n == 0) {
                    break;
                }
            }
        }

        return index;
    }

    // Returns the nth bookmark row in the list, regardless of other item types. The given value for
    // n determines which item is retrieved. If 1 is given then the first instance is returned, 2
    // will return the second, and so on.
    private ImprovedBookmarkRow getNthBookmarkRow(int n) {
        return getRowGeneric(ImprovedBookmarkRow.class, getNthBookmarkIndex(n));
    }

    // Same as #getNthBokmarkRow, but returns the view holder instead.
    private ViewHolder getNthBookmarkViewHolder(int n) {
        return getViewHolderAtIndex(getNthBookmarkIndex(n));
    }

    private <T extends View> T getRowGeneric(Class<T> clazz, int index) {
        View view = getViewHolderAtIndex(index).itemView;
        assertTrue(
                "Found " + view.getClass() + " expected " + clazz,
                clazz.isAssignableFrom(view.getClass()));
        return (T) view;
    }

    private ViewHolder getViewHolderAtIndex(int index) {
        return mItemsContainer.findViewHolderForAdapterPosition(index);
    }

    private void loadBookmarkModel() {
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.finishLoadingBookmarkModel(CallbackUtils.emptyRunnable());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mBookmarkModel.isBookmarkModelLoaded(), is(true));
                });
    }

    private void pollForModeAndCount(@BookmarkUiMode int uiMode, int itemCount) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mDelegate.getCurrentUiMode(), is(uiMode));
                    Criteria.checkThat(getBookmarkCount(), is(itemCount));
                });
    }

    private int getBookmarkCount() {
        int bookmarkCount = 0;
        for (ListItem item : mModelList) {
            if (item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_VISUAL
                    || item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_COMPACT) {
                bookmarkCount++;
            }
        }

        return bookmarkCount;
    }

    private void pressEscapeKey() throws ExecutionException {
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_ESCAPE);
        // Wait for any resulting UI updates to settle.
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
    }
}
