// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalToIgnoringCase;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Looper;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.chrome.browser.download.home.list.holder.ListItemViewHolder;
import org.chromium.chrome.browser.download.home.rename.RenameUtils;
import org.chromium.chrome.browser.download.home.toolbar.DownloadHomeToolbar;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.util.browser.edge_to_edge.TestEdgeToEdgeController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.List;

/** Tests the download home V2. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DownloadActivityV2Test {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static BlankUiTestActivity sActivity;

    @Mock private Tracker mTracker;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private DownloadHelpPageLauncher mHelpPageLauncher;
    @Spy private TestEdgeToEdgeController mEdgeToEdgeController;
    private @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier;

    private ModalDialogManager.Presenter mAppModalPresenter;

    private ModalDialogManager mModalDialogManager;

    private DownloadManagerCoordinator mDownloadCoordinator;

    private StubbedOfflineContentProvider mStubbedOfflineContentProvider;

    private DiscardableReferencePool mDiscardableReferencePool;

    /**
     * Returns a Matcher to find a particular {@link ViewHolder} that contains certain text.
     *
     * @param text The text that the view holder has in its view hierarchy.
     */
    private static Matcher<ViewHolder> hasTextInViewHolder(String text) {
        return new BoundedMatcher<>(ListItemViewHolder.class) {
            @Override
            public void describeTo(Description description) {
                description.appendText("has text: " + text);
            }

            @Override
            protected boolean matchesSafely(ListItemViewHolder listItemViewHolder) {
                ArrayList<View> outViews = new ArrayList<>();
                listItemViewHolder.itemView.findViewsWithText(
                        outViews, text, View.FIND_VIEWS_WITH_TEXT);
                return !outViews.isEmpty();
            }
        };
    }

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ModalDialogView.disableButtonTapProtectionForTesting();

        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(
                        any(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(
                        inv -> {
                            GURL url = inv.getArgument(0);
                            return url.getSpec();
                        });

        mStubbedOfflineContentProvider =
                new StubbedOfflineContentProvider() {
                    @Override
                    public void renameItem(ContentId id, String name, Callback<Integer> callback) {
                        new Handler(Looper.getMainLooper())
                                .post(() -> callback.onResult(handleRename(name)));
                    }
                };

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

        mDiscardableReferencePool = new DiscardableReferencePool();
    }

    private void setUpUi() {
        setUpUi(
                /* showDangerousItems= */ false,
                /* inlineSearchBar= */ false,
                /* autoFocusSearchBox= */ false);
    }

    private void setUpUi(
            boolean showDangerousItems, boolean inlineSearchBar, boolean autoFocusSearchBox) {
        // Initialize this here to avoid "wrong thread" assertion.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mEdgeToEdgeSupplier = new ObservableSupplierImpl<>());

        DownloadManagerUiConfig config =
                DownloadManagerUiConfigHelper.fromFlags(sActivity)
                        .setOtrProfileId(null)
                        .setIsSeparateActivity(true)
                        .setShowDangerousItems(showDangerousItems)
                        .setInlineSearchBar(inlineSearchBar)
                        .setAutoFocusSearchBox(autoFocusSearchBox)
                        .setEdgeToEdgePadAdjusterGenerator(
                                view ->
                                        EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                                view, mEdgeToEdgeSupplier))
                        .build();

        mAppModalPresenter = new AppModalPresenter(sActivity);

        mModalDialogManager =
                new ModalDialogManager(mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);

        FaviconProvider faviconProvider =
                new FaviconProvider() {
                    @Override
                    public void destroy() {}

                    @Override
                    public void getFavicon(
                            final String url, int faviconSizePx, Callback<Bitmap> callback) {}
                };
        Callback<Context> settingsNavigation = context -> {};
        ObservableSupplierImpl<Boolean> isPrefetchEnabledSupplier = new ObservableSupplierImpl<>();
        isPrefetchEnabledSupplier.set(true);

        mDownloadCoordinator =
                new DownloadManagerCoordinatorImpl(
                        sActivity,
                        config,
                        isPrefetchEnabledSupplier,
                        settingsNavigation,
                        mSnackbarManager,
                        mModalDialogManager,
                        mHelpPageLauncher,
                        mTracker,
                        faviconProvider,
                        mStubbedOfflineContentProvider,
                        mDiscardableReferencePool);
        sActivity.setContentView(mDownloadCoordinator.getView());
        BackPressHelper.create(
                sActivity,
                sActivity.getOnBackPressedDispatcher(),
                mDownloadCoordinator.getBackPressHandlers());

        mDownloadCoordinator.updateForUrl(UrlConstants.DOWNLOADS_URL);
    }

    // Adds a dangerous item in Download Home. Returns ID of the item.
    private ContentId setUpDangerousItem() {
        OfflineItem dangerousItem =
                StubbedProvider.createOfflineItem(
                        "offline_guid_5",
                        JUnitTestGURLs.URL_2,
                        OfflineItemState.COMPLETE,
                        1024,
                        "dangerous",
                        "/data/fake_path/Downloads/file_5",
                        System.currentTimeMillis(),
                        100000,
                        OfflineItemFilter.OTHER);
        dangerousItem.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        dangerousItem.isDangerous = true;
        dangerousItem.canRename = false;

        ThreadUtils.runOnUiThreadBlocking(
                () -> mStubbedOfflineContentProvider.addItem(dangerousItem));
        return dangerousItem.id;
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/372835715")
    public void testLaunchingActivity() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        // Shows activity title.
        onView(withText("Downloads")).check(matches(isDisplayed()));

        // Shows the list items.
        onView(withText("page 2")).check(matches(isDisplayed()));
        onView(withText("page 3")).check(matches(isDisplayed()));
        onView(withText("page 4")).check(matches(isDisplayed()));

        // The last item may be outside the view port, that recycler view won't create the view
        // holder, so scroll to that view holder first.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToHolder(hasTextInViewHolder("page 1")));
        onView(withText("page 1")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testTabsAreShown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });
        checkItemsDisplayed(true, true, true, true);

        Matcher filesTabMatcher = withText(equalToIgnoringCase("My Files"));
        Matcher prefetchTabMatcher = withText(equalToIgnoringCase("Explore Offline"));
        onView(filesTabMatcher).check(matches(isDisplayed()));
        onView(prefetchTabMatcher).check(matches(isDisplayed()));

        // Select Explore Offline tab, and verify the contents.
        onView(prefetchTabMatcher).perform(ViewActions.click());
        checkItemsDisplayed(false, false, false, false);

        // Select My files tab, and verify the contents.
        onView(filesTabMatcher).perform(ViewActions.scrollTo(), ViewActions.click());
        checkItemsDisplayed(true, true, true, true);
    }

    @Test
    @MediumTest
    public void testChipsAreShown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        // Two PAGES and two OTHER files. Should show All, Pages, and Other chips.
        checkItemsDisplayed(true, true, true, true);

        Matcher allChipMatcher =
                allOf(
                        withText(equalToIgnoringCase("All")),
                        isDescendantOfA(withId(R.id.content_container)));
        Matcher pagesChipMatcher =
                allOf(
                        withText(equalToIgnoringCase("Pages")),
                        isDescendantOfA(withId(R.id.content_container)));
        Matcher otherChipMatcher =
                allOf(
                        withText(equalToIgnoringCase("Other")),
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        onView(withId(R.id.empty_state_icon)).check(matches(not(isDisplayed())));

        // Go to Prefetch tab. It should be empty.
        onView(withText(equalToIgnoringCase("Explore Offline")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withText(containsString("Articles appear here"))).check(matches(isDisplayed()));
        onView(withId(R.id.empty_state_icon)).check(matches(isDisplayed()));

        // Go back to files tab. It shouldn't be empty.
        onView(withText(equalToIgnoringCase("My Files")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());
        onView(withId(R.id.empty_state_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testAddRemoveItems() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        String storageHeaderText = "Using 1.10 KB of";
        onView(withText(containsString(storageHeaderText))).check(matches(isDisplayed()));

        // Add an item. The new item should be visible and the storage text should be updated.
        OfflineItem item5 =
                StubbedProvider.createOfflineItem(
                        "offline_guid_5",
                        JUnitTestGURLs.URL_2,
                        OfflineItemState.COMPLETE,
                        1024,
                        "page 5",
                        "/data/fake_path/Downloads/file_5",
                        System.currentTimeMillis(),
                        100000,
                        OfflineItemFilter.OTHER);

        ThreadUtils.runOnUiThreadBlocking(() -> mStubbedOfflineContentProvider.addItem(item5));
        onView(withText("page 5")).check(matches(isDisplayed()));
        onView(withText(containsString("Using 2.10 KB of"))).check(matches(isDisplayed()));

        // Delete an item. The item should be gone and the storage text should be updated.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mStubbedOfflineContentProvider.removeItem(item5.id));
        onView(withText("page 5")).check(doesNotExist());
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddRemoveDangerousItem() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ true,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        String storageHeaderText = "Using 1.10 KB of";
        onView(withText(containsString(storageHeaderText))).check(matches(isDisplayed()));

        // Add a dangerous item. The new item should be visible and the storage text should not
        // include the size of the dangerous item.
        ContentId dangerousItemId = setUpDangerousItem();
        onView(withText("dangerous")).check(matches(isDisplayed()));
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
        onView(withText(containsString("Dangerous download blocked")))
                .check(matches(isDisplayed()));

        // Open menu for a dangerous download, it should not have share, rename options.
        onView(allOf(withId(R.id.more), hasSibling(withText("dangerous"))))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        onView(withText("Delete from history")).check(matches(isDisplayed()));
        onView(withText("Download")).check(matches(isDisplayed()));
        onView(withText("Rename")).check(doesNotExist());
        onView(withText("Share")).check(doesNotExist());

        // Delete the item. The item should be gone and the storage text should be unchanged.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mStubbedOfflineContentProvider.removeItem(dangerousItemId));
        onView(withText("dangerous")).check(doesNotExist());
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
        onView(withText(containsString("Dangerous download blocked"))).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testDangerousItemNotShownDueToConfig() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ false,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        String storageHeaderText = "Using 1.10 KB of";
        onView(withText(containsString(storageHeaderText))).check(matches(isDisplayed()));

        // Attempt to add a dangerous item. The new item should not be visible because the config
        // does not specify showDangerousItems.
        ContentId dangerousItemId = setUpDangerousItem();
        onView(withText("dangerous")).check(doesNotExist());
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
        onView(withText(containsString("Dangerous download blocked"))).check(doesNotExist());

        // Delete the item. Nothing should change because it was never displayed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mStubbedOfflineContentProvider.removeItem(dangerousItemId));
        onView(withText("dangerous")).check(doesNotExist());
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
        onView(withText(containsString("Dangerous download blocked"))).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/427410747")
    public void testDeleteDangerousUsingMenu() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ true,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        // Add a dangerous item.
        setUpDangerousItem();
        onView(withText("dangerous")).check(matches(isDisplayed()));

        // Delete a dangerous item using three dot menu. The item should be removed from the list.
        onView(allOf(withId(R.id.more), hasSibling(withText("dangerous"))))
                .perform(ViewActions.click());
        onView(withText("Delete from history"))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    onView(withText("dangerous")).check(doesNotExist());
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/427410747")
    public void testDeleteDangerousUsingSelection() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ true,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        // Add a dangerous item.
        setUpDangerousItem();
        // Long-press the dangerous item to select it.
        onView(withText("dangerous"))
                .check(matches(isDisplayed()))
                .perform(ViewActions.longClick());

        // Delete using the icon on the toolbar.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    DownloadHomeToolbar toolbar = sActivity.findViewById(R.id.download_toolbar);
                    toolbar.getMenu()
                            .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0);
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    onView(withText("dangerous")).check(doesNotExist());
                });
    }

    @Test
    @MediumTest
    public void testBypassDangerousWarning() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ true,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        String storageHeaderText = "Using 1.10 KB of";
        onView(withText(containsString(storageHeaderText))).check(matches(isDisplayed()));

        // Add a dangerous item.
        setUpDangerousItem();
        onView(withText("dangerous")).check(matches(isDisplayed()));
        onView(withText(containsString("Using 1.10 KB of"))).check(matches(isDisplayed()));
        // Open bypass dialog by clicking on the item.
        onView(withText(containsString("Dangerous download blocked")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        // Click the bypass button.
        onView(allOf(withId(R.id.negative_button), withText("Download anyway")))
                .inRoot(isDialog())
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        // Wait for the download to be validated.
        mStubbedOfflineContentProvider.getValidateDangerousDownloadHelper().waitForOnly();

        // The UI has updated that the download is no longer dangerous, and now counts towards the
        // storage total.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    onView(withText(containsString("Dangerous download blocked")))
                            .check(doesNotExist());
                });
        onView(withText(containsString("Using 2.10 KB of"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/423066352")
    public void testWarningBypassDialogLearnMore() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ true,
                            /* inlineSearchBar= */ false,
                            /* autoFocusSearchBox= */ false);
                });

        // Add a dangerous item.
        setUpDangerousItem();
        onView(withText("dangerous")).check(matches(isDisplayed()));
        // Open bypass dialog by clicking on the item.
        onView(withText(containsString("Dangerous download blocked")))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        // Click the Learn more button.
        onView(allOf(withId(R.id.positive_button), withText("Learn more")))
                .inRoot(isDialog())
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        Mockito.verify(mHelpPageLauncher).openUrl(eq(sActivity), anyString());
    }

    @Test
    @MediumTest
    public void testShowListItemMenuWithRename() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });
        onView(withText("page 3")).check(matches(isDisplayed()));

        // Open menu for a download, it should have share, delete, and rename options.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 3"))))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        onView(withText("Rename")).check(matches(isDisplayed()));
        onView(withText("Delete")).check(matches(isDisplayed()));
        if (DeviceInfo.isAutomotive()) {
            onView(withText("Share")).check(doesNotExist());
        } else {
            onView(withText("Share")).check(matches(isDisplayed()));
        }
    }

    @Test
    @MediumTest
    public void testShowListItemMenuWithoutRename() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        // The last item may be outside the view port, that recycler view won't create the view
        // holder, so scroll to that view holder first.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToHolder(hasTextInViewHolder("page 1")));

        // Open menu for a page download, it should have share, delete, but no rename option.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 1"))))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        onView(withText("Rename")).check(doesNotExist());
        onView(withText("Delete")).check(matches(isDisplayed()));
        if (DeviceInfo.isAutomotive()) {
            onView(withText("Share")).check(doesNotExist());
        } else {
            onView(withText("Share")).check(matches(isDisplayed()));
        }
    }

    @Test
    @MediumTest
    public void testShowToolbarMenu() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        // In non-selection state settings, search and close menu should be showing, the selection
        // toolbar should not exist.
        onView(withId(R.id.settings_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.search_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.close_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.selection_mode_number)).check(matches(not(isDisplayed())));
        onView(withId(R.id.selection_mode_share_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_delete_menu_id)).check(doesNotExist());

        // The last item may be outside the view port, that recycler view won't create the view
        // holder, so scroll to that view holder first.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToHolder(hasTextInViewHolder("page 1")));

        // Select an item.
        onView(withText("page 1")).perform(ViewActions.longClick());

        // Selection toolbar should be showing. Settings, search, and close menu should be gone.
        onView(withId(R.id.settings_menu_id)).check(doesNotExist());
        onView(withId(R.id.search_menu_id)).check(doesNotExist());
        onView(withId(R.id.close_menu_id)).check(doesNotExist());
        onView(withId(R.id.selection_mode_number)).check(matches(isDisplayed()));
        if (DeviceInfo.isAutomotive()) {
            onView(withId(R.id.selection_mode_share_menu_id)).check(matches(not(isDisplayed())));
        } else {
            onView(withId(R.id.selection_mode_share_menu_id)).check(matches(isDisplayed()));
        }
        onView(withId(R.id.selection_mode_delete_menu_id)).check(matches(isDisplayed()));

        // The last item may be outside the view port, that recycler view won't create the view
        // holder, so scroll to that view holder first.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToHolder(hasTextInViewHolder("page 1")));

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });
        SnackbarManager.setDurationForTesting(1);

        // The last item may be outside the view port, that recycler view won't create the view
        // holder, so scroll to that view holder first.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToHolder(hasTextInViewHolder("page 1")));

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

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    DownloadHomeToolbar toolbar = sActivity.findViewById(R.id.download_toolbar);
                    toolbar.getMenu()
                            .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0);
                });

        // The files tab should show empty view now.
        onView(withId(R.id.empty_state_icon)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1338140")
    public void testRenameItem() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        // Open menu for a list item, it should have the share option.
        onView(allOf(withId(R.id.more), hasSibling(withText("page 4"))))
                .perform(ViewActions.click());

        // Share an item. The share via android dialog should popup.
        if (DeviceInfo.isAutomotive()) {
            onView(withText("Share")).check(doesNotExist());
        } else {
            onView(withText("Share")).check(matches(isDisplayed()));
        }

        // TODO(shaktisahu): Perform a click, capture the Intent and check its contents.
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/372835715")
    public void testSearchView() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        final DownloadHomeToolbar toolbar = sActivity.findViewById(R.id.download_toolbar);
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
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

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/372252512")
    public void testDismissSearchViewByBackPress() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                });

        final DownloadHomeToolbar toolbar = sActivity.findViewById(R.id.download_toolbar);
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0));

        // The selection should be cleared when a search is started.
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Select an item and assert that the search view is no longer showing.
        onView(withText("page 4")).perform(ViewActions.longClick());
        onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));

        // Clear the selection by back press and assert that the search view is showing again.
        ThreadUtils.runOnUiThreadBlocking(sActivity.getOnBackPressedDispatcher()::onBackPressed);
        onView(withId(R.id.search_text)).check(matches(isDisplayed()));

        // Close the search view, by performing a back press.
        ThreadUtils.runOnUiThreadBlocking(sActivity.getOnBackPressedDispatcher()::onBackPressed);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    onView(withId(R.id.search_text)).check(matches(not(isDisplayed())));
                });
    }

    @Test
    @MediumTest
    public void testDownloadsFocus() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi(
                            /* showDangerousItems= */ false,
                            /* inlineSearchBar= */ true,
                            /* autoFocusSearchBox= */ true);
                });

        onView(withText("Downloads")).check(matches(isDisplayed()));
        // Check the search field is displayed
        onView(allOf(withId(R.id.search_text), isDescendantOfA(withId(R.id.download_search_bar))))
                .check(matches(isDisplayed()));
        // Check we can type in search query
        onView(allOf(withId(R.id.search_text), isDescendantOfA(withId(R.id.download_search_bar))))
                .perform(ViewActions.typeText("Google"));
        // Check no any downloaded item.
        onView(withText(containsString("Using 0.00 KB of"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testEdgeToEdge() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setUpUi();
                    // This should call SimpleEdgeToEdgePadAdjuster#mControllerChangedCallback.
                    mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
                });

        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        padAdjuster.overrideBottomInset(100);
        ViewGroup listView = mDownloadCoordinator.getListViewForTesting();
        assertEquals("Bottom insets should have been applied.", 100, listView.getPaddingBottom());
        assertFalse(listView.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals("Bottom insets should have been reset.", 0, listView.getPaddingBottom());
        assertTrue(listView.getClipToPadding());

        ThreadUtils.runOnUiThreadBlocking(mDownloadCoordinator::destroy);
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }

    /**
     * @param items The list (unsorted) of OfflineItems that could be displayed.
     * @param expectations Whether or not each item (1:1 with {@code items}) is visible.
     */
    private void checkItemsDisplayed(ArrayList<OfflineItem> items, List<Boolean> expectations) {
        int currentIndex = 2; // (1) Storage, (2) Filters

        Assert.assertEquals(items.size(), expectations.size());

        // Create a sortable pair of the OfflineItem and the visibility expectation.
        ArrayList<Pair<OfflineItem, Boolean>> sorted = new ArrayList<>();
        for (int i = 0; i < items.size(); i++) {
            sorted.add(Pair.create(items.get(i), expectations.get(i)));
        }

        // Sort by date, which is how the items will show up in the UI.
        Collections.sort(sorted, this::compareOfflineItems);

        for (int i = 0; i < sorted.size(); i++) {
            boolean visible = sorted.get(i).second;
            OfflineItem item = sorted.get(i).first;

            if (visible) {
                OfflineItem previous = findPreviousVisible(sorted, i);
                // If we have a day change, validate the header and move forward one item before
                // comparing.
                if (previous == null || ListUtils.compareItemByDate(previous, item) != 0) {
                    onView(withId(R.id.download_home_recycler_view))
                            .perform(RecyclerViewActions.scrollToPosition(currentIndex++));
                    onView(
                                    withText(
                                            StringUtils.dateToHeaderString(
                                                            new Date(
                                                                    sorted.get(i)
                                                                            .first
                                                                            .creationTimeMs))
                                                    .toString()))
                            .check(matches(isDisplayed()));
                }

                onView(withId(R.id.download_home_recycler_view))
                        .perform(RecyclerViewActions.scrollToPosition(currentIndex++));
                onView(withText(item.title)).check(matches(isDisplayed()));
            } else {
                onView(withText(item.title)).check(doesNotExist());
            }
        }

        // Reset the scroll position to the beginning to set proper expectations for the broader
        // test.
        onView(withId(R.id.download_home_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(0));
    }

    private OfflineItem findPreviousVisible(ArrayList<Pair<OfflineItem, Boolean>> list, int i) {
        for (int j = i - 1; j >= 0; j--) {
            if (list.get(j).second) return list.get(j).first;
        }
        return null;
    }

    private int compareOfflineItems(Pair<OfflineItem, Boolean> a, Pair<OfflineItem, Boolean> b) {
        return (int) (b.first.creationTimeMs - a.first.creationTimeMs);
    }

    private void checkItemsDisplayed(boolean item0, boolean item1, boolean item2, boolean item3) {
        checkItemsDisplayed(
                mStubbedOfflineContentProvider.getItemsSynchronously(),
                Arrays.asList(item0, item1, item2, item3));
    }

    private void renameFileAndVerifyErrorMessage(String name, int expectErrorMsgId) {
        onView(withId(R.id.file_name))
                .perform(ViewActions.clearText())
                .perform(ViewActions.typeText(name));

        onView(withId(R.id.positive_button))
                .check(matches(isDisplayed()))
                .perform(ViewActions.click());

        if (expectErrorMsgId != -1) {
            onView(withText(sActivity.getResources().getString(expectErrorMsgId)))
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
