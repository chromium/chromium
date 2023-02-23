// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightOff;
import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightPulse;

import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.AdapterDataObserver;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkActionBar;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderRow;
import org.chromium.chrome.browser.bookmarks.BookmarkItemRow;
import org.chromium.chrome.browser.bookmarks.BookmarkItemsAdapter;
import org.chromium.chrome.browser.bookmarks.BookmarkManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkRow;
import org.chromium.chrome.browser.bookmarks.BookmarkUIState;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkShoppingItemRow;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.ViewType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the bookmark manager.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(1406059): Disabling the shopping CPA should not be a requirement for these tests.
@Features.DisableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING})
@DoNotBatch(reason = "BookmarkTest has behaviours and thus can't be batched.")
public class BookmarkTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_PAGE_URL_GOOGLE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final String TEST_PAGE_TITLE_GOOGLE2 = "Google";
    private static final String TEST_PAGE_URL_FOO = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_TITLE_FOO = "Foo";
    private static final String TEST_FOLDER_TITLE = "Test folder";
    private static final String TEST_FOLDER_TITLE2 = "Test folder 2";
    private static final String TEST_TITLE_A = "a";
    private static final int TEST_PORT = 12345;

    private BookmarkManager mManager;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mItemsContainer;
    // Constant but can only be initialized after parameterized test runner setup because this would
    // trigger native load / CommandLineFlag setup.
    private GURL mTestUrlA;
    private GURL mTestPage;
    private GURL mTestPageFoo;
    private EmbeddedTestServer mTestServer;
    private @Nullable BookmarkActivity mBookmarkActivity;
    @Mock
    private SyncService mSyncService;
    private UserActionTester mActionTester;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();

            // Emulate sync disabled so promos are shown.
            when(mSyncService.isSyncRequested()).thenReturn(false);
            SyncService.overrideForTests(mSyncService);
        });
        // Use a custom port so the links are consistent for render tests.
        mActivityTestRule.getEmbeddedTestServerRule().setServerPort(TEST_PORT);
        mTestServer = mActivityTestRule.getTestServer();
        mTestUrlA = new GURL("http://a.com");
        mTestPage = new GURL(mTestServer.getURL(TEST_PAGE_URL_GOOGLE));
        mTestPageFoo = new GURL(mTestServer.getURL(TEST_PAGE_URL_FOO));
    }

    /**
     * Loads an empty partner bookmarks folder for testing. The partner bookmarks folder will appear
     * in the mobile bookmarks folder.
     *
     */
    private void loadEmptyPartnerBookmarksForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting(); });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    /**
     * Loads a non-empty partner bookmarks folder for testing. The partner bookmarks folder will
     * appear in the mobile bookmarks folder.
     */
    private void loadFakePartnerBookmarkShimForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.loadFakePartnerBookmarkShimForTesting(); });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    @After
    public void tearDown() throws Exception {
        if (mBookmarkActivity != null) ApplicationTestUtils.finishActivity(mBookmarkActivity);
        if (mActionTester != null) mActionTester.tearDown();
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    private void openBookmarkManager() throws InterruptedException {
        if (mActivityTestRule.getActivity().isTablet()) {
            mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_URL);
            mItemsContainer = mActivityTestRule.getActivity().findViewById(
                    R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mManager = ((BookmarkPage) mActivityTestRule.getActivity()
                                .getActivityTab()
                                .getNativePage())
                               .getManagerForTesting();
        } else {
            // phone
            mBookmarkActivity = ActivityTestUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), BookmarkActivity.class,
                    new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                            mActivityTestRule.getActivity(), R.id.all_bookmarks_menu_id));
            mItemsContainer = mBookmarkActivity.findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mManager = mBookmarkActivity.getManagerForTesting();
        }

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.getDragStateDelegate().setA11yStateForTesting(false));
    }

    void openRootFolder() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }

    void openMobileBookmarks() {
        openRootFolder();

        onView(withText("Mobile bookmarks")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private boolean isItemPresentInBookmarkList(final String expectedTitle) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                for (int i = 0; i < mItemsContainer.getAdapter().getItemCount(); i++) {
                    BookmarkId item = getIdByPosition(i);

                    if (item == null) continue;

                    String actualTitle = mBookmarkModel.getBookmarkTitle(item);
                    if (TextUtils.equals(actualTitle, expectedTitle)) {
                        return true;
                    }
                }
                return false;
            }
        });
    }

    @Test
    @SmallTest
    public void testAddBookmark() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        // Check partner bookmarks are lazily loaded.
        Assert.assertFalse(mBookmarkModel.isBookmarkModelLoaded());

        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkId id = mBookmarkModel.getUserBookmarkIdForTab(
                    mActivityTestRule.getActivity().getActivityTabProvider().get());
            Assert.assertTrue("The test page is not added as bookmark: ",
                    mBookmarkModel.doesBookmarkExist(id));
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            Assert.assertEquals(mBookmarkModel.getDefaultFolder(), item.getParentId());
            Assert.assertEquals(mTestPage, item.getUrl());
            Assert.assertEquals(TEST_PAGE_TITLE_GOOGLE, item.getTitle());
        });

        BookmarkTestUtil.waitForOfflinePageSaved(mTestPage);

        // Click the star button again to launch the edit activity.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForEditActivity().finish();
    }

    @Test
    @SmallTest
    public void testAddBookmarkToOtherFolder() {
        mActivityTestRule.loadUrl(mTestPage);
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        // Set default folder as "Other Folder".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT,
                    mBookmarkModel.getOtherFolderId().toString());
        });
        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkId id = mBookmarkModel.getUserBookmarkIdForTab(
                    mActivityTestRule.getActivity().getActivityTabProvider().get());
            Assert.assertTrue("The test page is not added as bookmark: ",
                    mBookmarkModel.doesBookmarkExist(id));
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            Assert.assertEquals("Bookmark added in a wrong default folder.",
                    mBookmarkModel.getOtherFolderId(), item.getParentId());
        });
    }

    @Test
    @SmallTest
    public void testOpenBookmark() throws InterruptedException, ExecutionException {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();
        openMobileBookmarks();

        Assert.assertTrue("Grid view does not contain added bookmark: ",
                isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
        final View title = getViewWithText(mItemsContainer, TEST_PAGE_TITLE_GOOGLE);
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(title));
        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(() -> {
            Tab activityTab = activity.getActivityTab();
            Criteria.checkThat(activityTab, Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.is(mTestPage));
        });
    }

    @Test
    @SmallTest
    public void testUrlComposition() {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkId mobileId = mBookmarkModel.getMobileFolderId();
            BookmarkId bookmarkBarId = mBookmarkModel.getDesktopFolderId();
            BookmarkId otherId = mBookmarkModel.getOtherFolderId();
            Assert.assertEquals("chrome-native://bookmarks/folder/" + mobileId,
                    BookmarkUIState.createFolderUrl(mobileId).toString());
            Assert.assertEquals("chrome-native://bookmarks/folder/" + bookmarkBarId,
                    BookmarkUIState.createFolderUrl(bookmarkBarId).toString());
            Assert.assertEquals("chrome-native://bookmarks/folder/" + otherId,
                    BookmarkUIState.createFolderUrl(otherId).toString());
        });
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShowBookmarkManager_Phone() throws InterruptedException {
        loadEmptyPartnerBookmarksForTesting();
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkUtils.showBookmarkManager(mActivityTestRule.getActivity(),
                    mBookmarkModel.getMobileFolderId(), /*isIncognito=*/false);
        });

        BookmarkTestUtil.waitForBookmarkActivity();

        // Assign so it's cleaned up after the test.
        mBookmarkActivity = (BookmarkActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testShowBookmarkManager_Tablet() throws InterruptedException {
        loadEmptyPartnerBookmarksForTesting();
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkUtils.showBookmarkManager(mActivityTestRule.getActivity(),
                    mBookmarkModel.getMobileFolderId(), /*isIncognito=*/false);
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> mActivityTestRule.getActivity().getActivityTab().getNativePage() != null
                        && mActivityTestRule.getActivity().getActivityTab().getNativePage()
                                        instanceof BookmarkPage);
    }

    @Test
    @SmallTest
    public void testopenBookmarkManagerFolder() throws InterruptedException {
        openBookmarkManager();
        openMobileBookmarks();

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        BookmarkDelegate delegate = getBookmarkManager();

        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        Assert.assertEquals("chrome-native://bookmarks/folder/3",
                BookmarkUtils.getLastUsedUrl(mActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testFolderNavigation_Phone() throws InterruptedException, ExecutionException {
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        final BookmarkDelegate delegate = getBookmarkManager();
        final BookmarkActionBar toolbar = ((BookmarkManager) delegate).getToolbarForTests();

        // Open the "Mobile bookmarks" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> delegate.openFolder(mBookmarkModel.getMobileFolderId()));

        // Check that we are in the mobile bookmarks folder.
        Assert.assertEquals("Mobile bookmarks", toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Open the new test folder.
        TestThreadUtils.runOnUiThreadBlocking(() -> delegate.openFolder(testFolder));

        // Check that we are in the editable test folder.
        Assert.assertEquals(TEST_FOLDER_TITLE, toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
        Assert.assertTrue(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(testFolder, TEST_FOLDER_TITLE2));

        // Check that the test folder reflects name changes.
        Assert.assertEquals(TEST_FOLDER_TITLE2, toolbar.getTitle());

        // Call BookmarkActionBar#onClick() to activate the navigation button.
        TestThreadUtils.runOnUiThreadBlocking(() -> toolbar.onClick(toolbar));

        // Check that we are back in the mobile folder
        Assert.assertEquals("Mobile bookmarks", toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Call BookmarkActionBar#onClick() to activate the navigation button.
        TestThreadUtils.runOnUiThreadBlocking(() -> toolbar.onClick(toolbar));

        // Check that we are in the root folder.
        Assert.assertEquals("Bookmarks", toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_NONE,
                toolbar.getNavigationButtonForTests());
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());
    }

    // TODO(twellington): Write a folder navigation test for tablets that waits for the Tab hosting
    //                    the native page to update its url after navigations.

    @Test
    @MediumTest
    public void testSearchBookmarks() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkDelegate delegate = getBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        Assert.assertEquals(
                "Wrong number of items before starting search.", 3, adapter.getItemCount());

        TestThreadUtils.runOnUiThreadBlocking(delegate::openSearchUI);

        Assert.assertEquals(BookmarkUIState.STATE_SEARCHING, delegate.getCurrentState());
        Assert.assertEquals(
                "Wrong number of items after showing search UI. The promo should be hidden.", 2,
                adapter.getItemCount());

        searchBookmarks("Google");
        Assert.assertEquals("Wrong number of items after searching.", 1,
                mItemsContainer.getAdapter().getItemCount());

        BookmarkId newBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE2, mTestPage);
        Assert.assertEquals("Wrong number of items after bookmark added while searching.", 2,
                mItemsContainer.getAdapter().getItemCount());

        removeBookmark(newBookmark);
        Assert.assertEquals("Wrong number of items after bookmark removed while searching.", 1,
                mItemsContainer.getAdapter().getItemCount());

        searchBookmarks("Non-existent page");
        Assert.assertEquals("Wrong number of items after searching for non-existent item.", 0,
                mItemsContainer.getAdapter().getItemCount());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> ((BookmarkManager) delegate).getToolbarForTests().hideSearchView());
        Assert.assertEquals("Wrong number of items after closing search UI.", 3,
                mItemsContainer.getAdapter().getItemCount());
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testSearchBookmarks_pressBack() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkDelegate delegate = getBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(
                Boolean.TRUE, getBookmarkManager().getHandleBackPressChangedSupplier().get());

        TestThreadUtils.runOnUiThreadBlocking(delegate::openSearchUI);

        Assert.assertEquals(BookmarkUIState.STATE_SEARCHING, delegate.getCurrentState());
        Assert.assertEquals(
                "Wrong number of items after showing search UI. The promo should be hidden.", 2,
                adapter.getItemCount());

        Assert.assertEquals(
                Boolean.TRUE, getBookmarkManager().getHandleBackPressChangedSupplier().get());

        // Exit search UI.
        TestThreadUtils.runOnUiThreadBlocking(
                mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        Assert.assertNotEquals(BookmarkUIState.STATE_SEARCHING, delegate.getCurrentState());

        // Enter search UI again.
        TestThreadUtils.runOnUiThreadBlocking(delegate::openSearchUI);

        searchBookmarks("Google");
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertEquals("Wrong number of items after searching.", 1,
                mItemsContainer.getAdapter().getItemCount());

        BookmarkRow itemView =
                (BookmarkRow) mItemsContainer.findViewHolderForLayoutPosition(0).itemView;

        toggleSelectionAndEndAnimation(getIdByPosition(0), itemView);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        TestThreadUtils.runOnUiThreadBlocking(
                mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);

        // Clear selection but still in search UI.
        CriteriaHelper.pollUiThread(
                () -> !itemView.isChecked(), "Expected item \"test\" to become not selected");
        Assert.assertEquals(BookmarkUIState.STATE_SEARCHING, delegate.getCurrentState());
        Assert.assertEquals(
                Boolean.TRUE, getBookmarkManager().getHandleBackPressChangedSupplier().get());

        // Exit search UI.
        TestThreadUtils.runOnUiThreadBlocking(
                mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());

        // Exit folder.
        Assert.assertEquals(
                Boolean.TRUE, getBookmarkManager().getHandleBackPressChangedSupplier().get());
        TestThreadUtils.runOnUiThreadBlocking(
                mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());

        // Exit bookmark activity.
        Assert.assertEquals(
                Boolean.FALSE, getBookmarkManager().getHandleBackPressChangedSupplier().get());
        TestThreadUtils.runOnUiThreadBlocking(
                mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testSearchBookmarks_Delete() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId testFolder2 = addFolder(TEST_FOLDER_TITLE2, testFolder);
        BookmarkId testBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, testFolder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder);
        openBookmarkManager();

        RecyclerView.Adapter adapter = getAdapter();
        BookmarkManager manager = getBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(testFolder);

        Assert.assertEquals("Wrong state, should be in folder", BookmarkUIState.STATE_FOLDER,
                manager.getCurrentState());
        Assert.assertEquals(
                "Wrong number of items before starting search.", 3, adapter.getItemCount());

        // Start searching without entering a query.
        TestThreadUtils.runOnUiThreadBlocking(manager::openSearchUI);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());

        // Select testFolder2 and delete it.
        toggleSelectionAndEndAnimation(testFolder2,
                (BookmarkRow) mItemsContainer.findViewHolderForLayoutPosition(2).itemView);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> manager.getToolbarForTests().onMenuItemClick(
                                manager.getToolbarForTests().getMenu().findItem(
                                        R.id.selection_mode_delete_menu_id)));

        // Should still be searching with the folder gone.
        Assert.assertEquals(
                "Wrong number of items.", 0, mItemsContainer.getAdapter().getItemCount());

        // Start searching, enter a query.
        searchBookmarks("Google");
        Assert.assertEquals("Wrong number of items after searching.", 1,
                mItemsContainer.getAdapter().getItemCount());

        // Remove the bookmark.
        removeBookmark(testBookmark);

        // The user should still be searching, and the bookmark should be gone.
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());
        Assert.assertEquals("Wrong number of items after searching.", 0,
                mItemsContainer.getAdapter().getItemCount());

        // Undo the deletion.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> manager.getUndoControllerForTests().onAction(null));

        // The user should still be searching, and the bookmark should reappear.
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());
        Assert.assertEquals("Wrong number of items after searching.", 1,
                mItemsContainer.getAdapter().getItemCount());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testSearchBookmarks_DeleteFolderWithChildrenInResults() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder);
        openBookmarkManager();

        RecyclerView.Adapter adapter = getAdapter();
        BookmarkManager manager = getBookmarkManager();

        // Start searching, enter a query.
        TestThreadUtils.runOnUiThreadBlocking(manager::openSearchUI);
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());
        searchBookmarks("test");
        Assert.assertEquals("Wrong number of items after searching.", 2,
                mItemsContainer.getAdapter().getItemCount());

        // Remove the bookmark.
        removeBookmark(testFolder);

        // The user should still be searching, and the bookmark should be gone.
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());
        Assert.assertEquals("Wrong number of items after searching.", 0,
                mItemsContainer.getAdapter().getItemCount());

        // Undo the deletion.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> manager.getUndoControllerForTests().onAction(null));

        // The user should still be searching, and the bookmark should reappear.
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                manager.getCurrentState());
        Assert.assertEquals("Wrong number of items after searching.", 2,
                mItemsContainer.getAdapter().getItemCount());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testBookmarkFolderIcon(boolean nightModeEnabled) throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        openMobileBookmarks();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkManager manager = getBookmarkManager();

        mRenderTestRule.render(manager.getView(), "bookmark_manager_one_folder");

        BookmarkRow itemView = (BookmarkRow) manager.getRecyclerViewForTests()
                                       .findViewHolderForAdapterPosition(0)
                                       .itemView;

        toggleSelectionAndEndAnimation(getIdByPosition(0), itemView);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(manager.getView(), "bookmark_manager_folder_selected");
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Tablets don't have a close button.
    public void testCloseBookmarksWhileStillLoading() throws Exception {
        BookmarkManager.preventLoadingForTesting(true);

        openBookmarkManager();

        final BookmarkActionBar toolbar = mManager.getToolbarForTests();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.onMenuItemClick(toolbar.getMenu().findItem(R.id.close_menu_id)));

        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);

        BookmarkManager.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    public void testEditHiddenWhileStillLoading() throws Exception {
        BookmarkManager.preventLoadingForTesting(true);

        openBookmarkManager();

        BookmarkActionBar toolbar = mManager.getToolbarForTests();
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        BookmarkManager.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSelectionMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_TITLE_A, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.getDragHandleViewForTests();

        BookmarkRow testFolderA =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = testFolderA.findViewById(R.id.more);
        View aDragHandle = testFolderA.getDragHandleViewForTests();

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected bookmark toolbar to be selection mode",
                mManager.getToolbarForTests().getCurrentViewType(), ViewType.SELECTION_VIEW);
        Assert.assertEquals("Expected more button of selected item to be gone when drag is active.",
                View.GONE, testMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of selected item to be visible when drag is active.",
                View.VISIBLE, testDragHandle.getVisibility());
        Assert.assertTrue("Expected drag handle to be enabled when drag is active.",
                testDragHandle.isEnabled());

        Assert.assertEquals(
                "Expected more button of unselected item to be gone when drag is active.",
                View.GONE, aMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of unselected item to be visible when drag is active.",
                View.VISIBLE, aDragHandle.getVisibility());
        Assert.assertFalse(
                "Expected drag handle of unselected item to be disabled when drag is active.",
                aDragHandle.isEnabled());
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSearchMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_TITLE_A);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.getDragHandleViewForTests();

        BookmarkRow a = (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = a.findViewById(R.id.more);
        View aDragHandle = a.getDragHandleViewForTests();

        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected drag handle of selected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, testDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of selected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, testMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of selected item to be disabled "
                        + "when selection mode is activated from search.",
                testMoreButton.isEnabled());

        Assert.assertEquals("Expected drag handle of unselected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, aDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of unselected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, aMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of unselected item to be disabled "
                        + "when selection mode is activated from search.",
                aMoreButton.isEnabled());
    }

    @Test
    @MediumTest
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder()).subList(0, 3));
        });

        expected.add(fooId);
        expected.add(aId);
        expected.add(googleId);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.addObserver(bookmarkModelObserver); });

        BookmarkRow foo =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(3).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_FOO, foo.getTitle());
        toggleSelectionAndEndAnimation(fooId, foo);

        // Starts as last bookmark (2nd index) and ends as 0th bookmark (promo header not included).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(3, 1);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder());
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
            Assert.assertTrue("The selected item should stay selected", foo.isItemSelected());
        });
    }

    @Test
    @MediumTest
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder()).subList(0, 4));
        });

        expected.add(cId);
        expected.add(bId);
        expected.add(aId);
        expected.add(testId);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.addObserver(bookmarkModelObserver); });

        BookmarkFolderRow test =
                (BookmarkFolderRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionAndEndAnimation(testId, test);

        // Starts as 0th bookmark (not counting promo header) and ends as last (index 3).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 4);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder());
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 4));
            Assert.assertTrue("The selected item should stay selected", test.isItemSelected());
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder()).subList(0, 3));
        });

        expected.add(bId);
        expected.add(testId);
        expected.add(aId);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.addObserver(bookmarkModelObserver); });

        BookmarkFolderRow test =
                (BookmarkFolderRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionAndEndAnimation(testId, test);

        // Starts as 0th bookmark (not counting promo header) and ends at the 1st index.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 2);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder());
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
            Assert.assertTrue("The selected item should stay selected", test.isItemSelected());
        });
    }

    @Test
    @MediumTest
    public void testPromoDraggability() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        ViewHolder promo = mItemsContainer.findViewHolderForAdapterPosition(0);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        BookmarkItemsAdapter adapter = ((BookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Promo header should not be passively draggable",
                adapter.isPassivelyDraggable(promo));
        Assert.assertFalse("Promo header should not be actively draggable",
                adapter.isActivelyDraggable(promo));
    }

    @Test
    @MediumTest
    public void testPartnerFolderDraggability() throws Exception {
        BookmarkId testId = addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        ViewHolder partner = mItemsContainer.findViewHolderForAdapterPosition(2);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        BookmarkItemsAdapter adapter = ((BookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Partner bookmarks folder should not be passively draggable",
                adapter.isPassivelyDraggable(partner));
        Assert.assertFalse("Partner bookmarks folder should not be actively draggable",
                adapter.isActivelyDraggable(partner));
    }

    @Test
    @MediumTest
    public void testUnselectedItemDraggability() throws Exception {
        BookmarkId aId = addBookmark("a", mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        ViewHolder test = mItemsContainer.findViewHolderForAdapterPosition(1);
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) test.itemView).getTitle());

        toggleSelectionAndEndAnimation(
                aId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView);

        BookmarkItemsAdapter adapter = ((BookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertTrue("Unselected rows should be passively draggable",
                adapter.isPassivelyDraggable(test));
        Assert.assertFalse("Unselected rows should not be actively draggable",
                adapter.isActivelyDraggable(test));
    }

    @Test
    @MediumTest
    public void testCannotSelectPromo() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        View promo = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        TouchCommon.longPressView(promo);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on promo view.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testCannotSelectPartner() throws Exception {
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        View partner = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        TouchCommon.longPressView(partner);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on partner bookmark.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testMoveUpMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);

        openBookmarkManager();
        openMobileBookmarks();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownGoneForBottomElement() throws Exception {
        addBookmarkWithPartner(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveUpGoneForTopElement() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1046653")
    public void testMoveButtonsGoneInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);
        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        View more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneWithOneBookmark() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        View more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneForPartnerBookmarks() throws Exception {
        loadFakePartnerBookmarkShimForTesting();
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open partner bookmarks folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getPartnerFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        Assert.assertEquals("Wrong number of items in partner bookmark folder.", 2,
                getAdapter().getItemCount());

        // Verify that bookmark 1 is editable (so more button can be triggered) but not movable.
        BookmarkId partnerBookmarkId1 = getReorderAdapter().getIdByPosition(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkItem partnerBookmarkItem1 = mBookmarkModel.getBookmarkById(partnerBookmarkId1);
            partnerBookmarkItem1.forceEditableForTesting();
            Assert.assertEquals("Incorrect bookmark type for item 1", BookmarkType.PARTNER,
                    partnerBookmarkId1.getType());
            Assert.assertFalse("Partner item 1 should not be movable",
                    BookmarkUtils.isMovable(partnerBookmarkItem1));
            Assert.assertTrue(
                    "Partner item 1 should be editable", partnerBookmarkItem1.isEditable());
        });

        // Verify that bookmark 2 is editable (so more button can be triggered) but not movable.
        View partnerBookmarkView1 = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        View more1 = partnerBookmarkView1.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more1::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());

        // Verify that bookmark 2 is not movable.
        BookmarkId partnerBookmarkId2 = getReorderAdapter().getIdByPosition(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkItem partnerBookmarkItem2 = mBookmarkModel.getBookmarkById(partnerBookmarkId2);
            partnerBookmarkItem2.forceEditableForTesting();
            Assert.assertEquals("Incorrect bookmark type for item 2", BookmarkType.PARTNER,
                    partnerBookmarkId2.getType());
            Assert.assertFalse("Partner item 2 should not be movable",
                    BookmarkUtils.isMovable(partnerBookmarkItem2));
            Assert.assertTrue(
                    "Partner item 2 should be editable", partnerBookmarkItem2.isEditable());
        });

        // Verify that bookmark 2 does not have move up/down items.
        View partnerBookmarkView2 = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View more2 = partnerBookmarkView2.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more2::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testTopLevelFolderUpdateAfterSync() throws Exception {
        // Set up the test and open the bookmark manager to the Mobile Bookmarks folder.
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        openBookmarkManager();
        BookmarkItemsAdapter adapter = getReorderAdapter();

        // Add a bookmark to the Other Bookmarks folder.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull(mBookmarkModel.addBookmark(
                    mBookmarkModel.getOtherFolderId(), 0, TEST_TITLE_A, mTestUrlA));
        });

        TestThreadUtils.runOnUiThreadBlocking(adapter::simulateSignInForTests);
        Assert.assertEquals(
                "Expected promo, \"Reading List\", \"Mobile Bookmarks\" and \"Other Bookmarks\" folder to appear!",
                4, adapter.getItemCount());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1369091")
    public void testShowInFolder_NoScroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        openMobileBookmarks();

        // Enter search mode.
        enterSearch();

        // Click "Show in folder".
        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());

        // Assert that the view pulses.
        Assert.assertTrue("Expected bookmark row to pulse after clicking \"show in folder\"!",
                checkHighlightPulse(testFolder));

        // Enter search mode again.
        enterSearch();

        Assert.assertTrue("Expected bookmark row to not be highlighted "
                        + "after entering search mode",
                checkHighlightOff(testFolder));

        // Click "Show in folder" again.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());
        Assert.assertTrue(
                "Expected bookmark row to pulse after clicking \"show in folder\" a 2nd time!",
                checkHighlightPulse(testFolder));
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1333546
    public void testShowInFolder_Scroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE); // Index 8
        addBookmark(TEST_TITLE_A, mTestUrlA);
        addBookmark(TEST_PAGE_TITLE_FOO, new GURL("http://foo.com"));
        addFolder(TEST_PAGE_TITLE_GOOGLE2);
        addFolder("B");
        addFolder("C");
        addFolder("D");
        addFolder("E"); // Index 1
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.onSearchTextChanged(TEST_FOLDER_TITLE));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());

        // This should be in the 8th position now.
        ViewHolder testFolderInList = mItemsContainer.findViewHolderForAdapterPosition(8);
        Assert.assertFalse(
                "Expected list to scroll bookmark item into view", testFolderInList == null);
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolderInList.itemView).getTitle());
        Assert.assertTrue("Expected highlight to pulse on after scrolling to the item!",
                checkHighlightPulse(testFolderInList.itemView));
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1333546
    public void testShowInFolder_OpenOtherFolder() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(testId, 0, TEST_TITLE_A, mTestUrlA));
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.onSearchTextChanged(mTestUrlA.getSpec()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_TITLE_A);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Make sure that we're in the right folder (index 1 because of promo).
        View itemA = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_TITLE_A,
                ((BookmarkItemRow) itemA).getTitle());

        Assert.assertTrue("Expected highlight to pulse after opening an item in another folder!",
                checkHighlightPulse(itemA));

        // Open mobile bookmarks folder, then go back to the subfolder.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.openFolder(mBookmarkModel.getMobileFolderId());
            mManager.openFolder(testId);
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        View itemASecondView = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_TITLE_A,
                ((BookmarkItemRow) itemASecondView).getTitle());
        Assert.assertTrue(
                "Expected highlight to not be highlighted after exiting and re-entering folder!",
                checkHighlightOff(itemASecondView));
    }

    @Test
    @SmallTest
    public void testAddBookmarkInBackgroundWithSelection() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(1, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(id, row);
        CallbackHelper helper = new CallbackHelper();
        getAdapter().registerAdapterDataObserver(new AdapterDataObserver() {
            @Override
            public void onChanged() {
                helper.notifyCalled();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addBookmark(folder, 1, TEST_PAGE_TITLE_GOOGLE, mTestPage);
        });

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getAdapter().getItemCount());
            Assert.assertTrue("The selected row should be kept selected", row.isItemSelected());
        });
    }

    @Test
    @SmallTest
    public void testDeleteAllSelectedBookmarksInBackground() throws Exception {
        // Select one bookmark and then remove that in background.
        // In the meantime, the toolbar changes from selection mode to normal mode.
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        BookmarkId aId = addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(3, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        toggleSelectionAndEndAnimation(googleId, row);
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
        });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    "Item is not deleted", isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getReorderAdapter().getItemCount());
            Assert.assertEquals("Bookmark View should be back to normal view",
                    mManager.getToolbarForTests().getCurrentViewType(), ViewType.NORMAL_VIEW);
        });
    }

    @Test
    @SmallTest
    public void testDeleteSomeSelectedBookmarksInBackground() throws Exception {
        // selected on bookmarks and then remove one of them in background
        // in the meantime, the toolbar stays in selection mode
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        BookmarkId aId = addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(3, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        toggleSelectionAndEndAnimation(googleId, row);
        BookmarkRow aRow =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(aId, aRow);
        CallbackHelper helper = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
        });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    "Item is not deleted", isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getReorderAdapter().getItemCount());
            Assert.assertTrue("Item selected should not be cleared", aRow.isItemSelected());
            Assert.assertEquals("Should stay in selection mode because there is one selected",
                    mManager.getToolbarForTests().getCurrentViewType(), ViewType.SELECTION_VIEW);
        });
    }

    @Test
    @SmallTest
    public void testUpdateSelectedBookmarkInBackground() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        Assert.assertEquals(1, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(id, row);
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addObserver(new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        helper.notifyCalled();
                    }
                }));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(id, TEST_PAGE_TITLE_GOOGLE));

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(1, getAdapter().getItemCount());
            Assert.assertTrue("The selected row should stay selected", row.isItemSelected());
        });
    }

    @Test
    @MediumTest
    public void testBookmarksDoesNotRecordLaunchMetrics() throws Throwable {
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();

        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        openBookmarkManager();
        openMobileBookmarks();

        onView(withText(TEST_PAGE_TITLE_GOOGLE)).perform(click());
        BookmarkTestUtil.waitForTabbedActivity();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));
    }

    /**
     * Test that we record Bookmarks.BookmarkTestUtil.openBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in regular mode.
     *
     * Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via BookmarkTestUtil.openBookmarkManager test method which circumvents the
     * click of R.id.all_bookmarks_menu_id, this doesn't happen in actual case and the metric indeed
     * gets recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testRecordsHistogramWhenBookmarkManagerOpened_InRegular() throws Throwable {
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.REGULAR));
    }

    /**
     * Test that we record Bookmarks.OpenBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in Incognito mode.
     *
     * Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via openBookmarkManager test method which circumvents the click of
     * R.id.all_bookmarks_menu_id. This doesn't happen in actual case and the metric indeed gets
     * recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testRecordsHistogramWhenBookmarkManagerOpened_InIncognito() throws Throwable {
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);
        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.INCOGNITO));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarksVisualRefreshFolders() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        openMobileBookmarks();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkManager manager = getBookmarkManager();

        mRenderTestRule.render(manager.getView(), "bookmarks_visual_refresh_folders");
        BookmarkRow itemView = (BookmarkRow) manager.getRecyclerViewForTests()
                                       .findViewHolderForAdapterPosition(0)
                                       .itemView;

        toggleSelectionAndEndAnimation(getIdByPosition(0), itemView);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(manager.getView(), "bookmarks_visual_refresh_folders_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarksVisualRefreshBookmarksAndFolder() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        openMobileBookmarks();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkManager manager = getBookmarkManager();

        mRenderTestRule.render(manager.getView(), "bookmarks_visual_refresh_bookmarksandfolders");
        BookmarkRow itemView1 = (BookmarkRow) manager.getRecyclerViewForTests()
                                        .findViewHolderForAdapterPosition(0)
                                        .itemView;
        BookmarkRow itemView2 = (BookmarkRow) manager.getRecyclerViewForTests()
                                        .findViewHolderForAdapterPosition(1)
                                        .itemView;

        toggleSelectionAndEndAnimation(getIdByPosition(0), itemView1);
        toggleSelectionAndEndAnimation(getIdByPosition(1), itemView2);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView1::isChecked, "Expected item \"test\" to become selected");
        CriteriaHelper.pollUiThread(
                itemView2::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(
                manager.getView(), "bookmarks_compact_visual_refresh_bookmarksandfolders_selected");
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.EnableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testShoppingFilterInBookmarks() throws InterruptedException, ExecutionException {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.openFolder(mBookmarkModel.getRootFolderId()); });

        onView(withText("Tracked products")).perform(click());
        final BookmarkDelegate delegate = getBookmarkManager();
        final BookmarkActionBar toolbar = ((BookmarkManager) delegate).getToolbarForTests();

        // Check that we are in the mobile bookmarks folder.
        Assert.assertEquals("Tracked products", toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.EnableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testShoppingDataPresentButFeatureDisabled()
            throws InterruptedException, ExecutionException {
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder().setShoppingSpecifics(
                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBookmarkModel.setPowerBookmarkMeta(id, meta.build()); });
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        RecyclerView.Adapter adapter = getAdapter();
        final BookmarkManager manager = getBookmarkManager();

        BookmarkRow itemView = (BookmarkRow) manager.getRecyclerViewForTests()
                                       .findViewHolderForAdapterPosition(0)
                                       .itemView;
        Assert.assertNotEquals(PowerBookmarkShoppingItemRow.class, itemView.getClass());
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
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }

    /**
     * Adds a folder in the scenario where we have partner bookmarks.
     *
     * @param title The title of the folder to add.
     * @return The BookmarkId of the added folder.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addFolderWithPartner(String title) throws ExecutionException {
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    private BookmarkItemsAdapter getReorderAdapter() {
        return (BookmarkItemsAdapter) getAdapter();
    }

    private void enterSearch() throws Exception {
        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);
        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");
    }

    private void clickMoreButtonOnFirstItem(String expectedBookmarkItemTitle) throws Exception {
        View firstItem = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", expectedBookmarkItemTitle,
                firstItem instanceof BookmarkItemRow ? ((BookmarkItemRow) firstItem).getTitle()
                                                     : ((BookmarkFolderRow) firstItem).getTitle());
        View more = firstItem.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::performClick);
    }

    /**
     * Returns the View that has the given text.
     *
     * @param viewGroup    The group to which the view belongs.
     * @param expectedText The expected description text.
     * @return The unique view, if one exists. Throws an exception if one doesn't exist.
     */
    private static View getViewWithText(final ViewGroup viewGroup, final String expectedText) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<View>() {
            @Override
            public View call() {
                ArrayList<View> outViews = new ArrayList<>();
                ArrayList<View> matchingViews = new ArrayList<>();
                viewGroup.findViewsWithText(outViews, expectedText, View.FIND_VIEWS_WITH_TEXT);
                // outViews includes all views whose text contains expectedText as a
                // case-insensitive substring. Filter these views to find only exact string matches.
                for (View v : outViews) {
                    if (TextUtils.equals(((TextView) v).getText().toString(), expectedText)) {
                        matchingViews.add(v);
                    }
                }
                Assert.assertEquals("Exactly one item should be present.", 1, matchingViews.size());
                return matchingViews.get(0);
            }
        });
    }

    private void toggleSelectionAndEndAnimation(BookmarkId id, BookmarkRow view) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.getSelectionDelegate().toggleSelectionForItem(id);
            view.endAnimationsForTests();
            mManager.getToolbarForTests().endAnimationsForTesting();
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }

    private BookmarkId addBookmark(final String title, GURL url, BookmarkId parent)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(parent, 0, title, url));
    }

    private BookmarkId addBookmark(final String title, final GURL url) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }

    private BookmarkId addFolder(final String title) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    private BookmarkId addFolder(final String title, BookmarkId parent) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(parent, 0, title));
    }

    private void removeBookmark(final BookmarkId bookmarkId) {
        TestThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.deleteBookmark(bookmarkId));
    }

    private RecyclerView.Adapter getAdapter() {
        return mItemsContainer.getAdapter();
    }

    private BookmarkManager getBookmarkManager() {
        return (BookmarkManager) getReorderAdapter().getDelegateForTesting();
    }

    private BookmarkId getIdByPosition(int pos) {
        return getReorderAdapter().getIdByPosition(pos);
    }

    private void searchBookmarks(final String query) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getReorderAdapter().search(query));
    }

    private void openFolder(BookmarkId folder) {
        final BookmarkDelegate delegate = getBookmarkManager();
        TestThreadUtils.runOnUiThreadBlocking(() -> delegate.openFolder(folder));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }
}
