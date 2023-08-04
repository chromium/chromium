// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Tests for the bookmark save flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.SHOPPING_LIST)
@Features.DisableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
public class BookmarkSaveFlowTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private ShoppingService mShoppingService;
    @Mock
    private PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Mock
    private UserEducationHelper mUserEducationHelper;

    private BookmarkSaveFlowCoordinator mBookmarkSaveFlowCoordinator;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;
    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                mActivityTestRule.getActivity());

        // Setup price-tracking.
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mBottomSheetController =
                    cta.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
            mBookmarkSaveFlowCoordinator =
                    new BookmarkSaveFlowCoordinator(cta, mBottomSheetController, mShoppingService,
                            mUserEducationHelper, Profile.getLastUsedRegularProfile());
        });

        loadBookmarkModel();
        doAnswer((invocation) -> {
            ((Callback<Boolean>) invocation.getArgument(3)).onResult(true);
            return null;
        })
                .when(mMockPriceTrackingUtilsJni)
                .setPriceTrackingStateForBookmark(
                        any(Profile.class), anyLong(), anyBoolean(), any(), anyBoolean());
        doAnswer((invocation) -> {
            ((Callback<Boolean>) invocation.getArgument(1)).onResult(true);
            return null;
        })
                .when(mShoppingService)
                .subscribe(any(CommerceSubscription.class), any());
        doAnswer((invocation) -> {
            ((Callback<Boolean>) invocation.getArgument(1)).onResult(true);
            return null;
        })
                .when(mShoppingService)
                .unsubscribe(any(CommerceSubscription.class), any());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id);
            return null;
        });
        mRenderTestRule.render(
                mBookmarkSaveFlowCoordinator.getViewForTesting(), "bookmark_save_flow");
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlow_DestroyAfterHidden() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id);
            mBookmarkSaveFlowCoordinator.close();
            return null;
        });
        CriteriaHelper.pollUiThread(
                ()
                        -> mBookmarkSaveFlowCoordinator.getIsDestroyedForTesting(),
                "Save flow coordinator not destroyed.");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_BookmarkMoved() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id, /*fromExplicitTrackUi=*/false,
                    /*wasBookmarkMoved=*/true,
                    /*isNewBookmark=*/false);
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_bookmark_moved");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder().setShoppingSpecifics(
                    ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
            mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false, /*isNewBookmark=*/true, meta.build());
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem_fromHeuristicEntryPoint()
            throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder().setShoppingSpecifics(
                    ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
            mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/true,
                    /*wasBookmarkMoved=*/false, /*isNewBookmark=*/false, meta.build());
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item_from_heuristic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem_fromHeuristicEntryPoint_saveFailed()
            throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder().setShoppingSpecifics(
                    ShoppingSpecifics.newBuilder().setProductClusterId(1234L));
            mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false, /*isNewBookmark=*/false, meta.build());
            return null;
        });
        doAnswer((invocation) -> {
            ((Callback<Boolean>) invocation.getArgument(3)).onResult(false);
            return null;
        })
                .when(mMockPriceTrackingUtilsJni)
                .setPriceTrackingStateForBookmark(
                        any(Profile.class), anyLong(), anyBoolean(), any(), anyBoolean());
        doAnswer((invocation) -> {
            ((Callback<Boolean>) invocation.getArgument(1)).onResult(false);
            return null;
        })
                .when(mShoppingService)
                .subscribe(any(CommerceSubscription.class), any());
        onView(withId(R.id.notification_switch)).perform(click());
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item_from_heuristic_save_failed");
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlowEdit() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false,
                    /*isNewBookmark=*/true);
            return null;
        });
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ClickUtils.clickButton(cta.findViewById(R.id.bookmark_edit));
        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.edit_bookmark)))
                .check(matches(isDisplayed()));

        // Dismiss the activity.
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlowChooseFolder() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false,
                    /*isNewBookmark=*/true);
            return null;
        });
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ClickUtils.clickButton(cta.findViewById(R.id.bookmark_select_folder));
        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.bookmark_choose_folder)))
                .check(matches(isDisplayed()));

        // Dismiss the activity.
        Espresso.pressBack();
    }

    private void loadBookmarkModel() {
        // Do not read partner bookmarks in setUp(), so that the lazy reading is covered.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PartnerBookmarksShim.kickOffReading(mActivityTestRule.getActivity()));
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    private BookmarkId addBookmark(final String title, final GURL url) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }
}
