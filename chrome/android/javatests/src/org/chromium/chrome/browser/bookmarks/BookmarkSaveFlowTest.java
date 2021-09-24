// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Tests for the bookmark save flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
public class BookmarkSaveFlowTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public DisableAnimationsTestRule mDisableAnimationsTestRule = new DisableAnimationsTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private BookmarkSaveFlowCoordinator mBookmarkSaveFlowCoordinator;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;
    private BookmarkModel mBookmarkModel;
    private BookmarkBridge mBookmarkBridge;

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                mActivityTestRule.getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mBottomSheetController =
                    cta.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mBookmarkSaveFlowCoordinator =
                    new BookmarkSaveFlowCoordinator(cta, mBottomSheetController);
            mBookmarkModel = new BookmarkModel(Profile.fromWebContents(
                    mActivityTestRule.getActivity().getActivityTab().getWebContents()));
            mBookmarkBridge = mActivityTestRule.getActivity().getBookmarkBridgeForTesting();
        });

        loadBookmarkModel();
        BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
        TestThreadUtils.runOnUiThreadBlocking(() -> { mBookmarkSaveFlowCoordinator.show(id); });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow() throws IOException {
        mRenderTestRule.render(
                mBookmarkSaveFlowCoordinator.getViewForTesting(), "bookmark_save_flow");
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlowEdit() throws IOException {
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