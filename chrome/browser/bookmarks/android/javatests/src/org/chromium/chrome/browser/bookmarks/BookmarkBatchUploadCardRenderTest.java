// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "SyncTestRule doesn't support batching.")
@EnableFeatures({ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP})
public class BookmarkBatchUploadCardRenderTest {
    private static final int RENDER_TEST_REVISION = 2;

    @Rule public final SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule public final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel =
                            BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
                    mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBookmarkBatchUploadEntryDescriptionBookmark() throws Exception {
        runOnUiThreadBlocking(
                () ->
                        // Add local bookmark.
                        mBookmarkModel.addBookmark(
                                mBookmarkModel.getMobileFolderId(),
                                0,
                                "bookmark",
                                new GURL("https://test.com")));

        mSyncTestRule.setUpAccountAndSignInForTesting();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.selectable_list_recycler_view))
                .perform(RecyclerViewActions.scrollToLastPosition());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return getBookmarkHostActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_bookmark");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBookmarkBatchUploadEntryDescriptionOther() throws Exception {
        runOnUiThreadBlocking(
                () -> // Add local reading list entry.
                mBookmarkModel.addToDefaultReadingList(
                                "reading list entry", new GURL("https://test.com")));

        mSyncTestRule.setUpAccountAndSignInForTesting();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.selectable_list_recycler_view))
                .perform(RecyclerViewActions.scrollToLastPosition());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return getBookmarkHostActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_other");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBookmarkBatchUploadEntryDescriptionBookmarkAndOther() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    // Add local bookmark.
                    mBookmarkModel.addBookmark(
                            mBookmarkModel.getMobileFolderId(),
                            0,
                            "bookmark",
                            new GURL("https://test.com"));
                    // Add local reading list entry.
                    mBookmarkModel.addToDefaultReadingList(
                            "reading list entry", new GURL("https://test.com"));
                });

        mSyncTestRule.setUpAccountAndSignInForTesting();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.selectable_list_recycler_view))
                .perform(RecyclerViewActions.scrollToLastPosition());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return getBookmarkHostActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_bookmark_and_other");
    }

    // Get the activity that hosts the bookmark UI - on phones, this is a BookmarkActivity, on
    // tablets this is a native page.
    private Activity getBookmarkHostActivity() {
        if (mActivityTestRule.getActivity().isTablet()) {
            return mActivityTestRule.getActivity();
        } else {
            return mBookmarkTestRule.getBookmarkActivity();
        }
    }
}
