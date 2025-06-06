// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.view.View;

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
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "SyncTestRule doesn't support batching.")
@EnableFeatures({ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP})
public class BookmarkBatchUploadCardRenderTest {
    private static final int RENDER_TEST_REVISION = 1;

    @Rule public final SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule public final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

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
        SyncTestUtil.waitForSyncTransportActive();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return mBookmarkTestRule
                                    .getBookmarkActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_bookmark");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBookmarkBatchUploadEntryDescriptionOther() throws Exception {
        // Add a local bookmark.
        runOnUiThreadBlocking(
                () -> // Add local reading list entry.
                mBookmarkModel.addToDefaultReadingList(
                                "reading list entry", new GURL("https://test.com")));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return mBookmarkTestRule
                                    .getBookmarkActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_other");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBookmarkBatchUploadEntryDescriptionBookmarkAndOther() throws Exception {
        // Add a local bookmark.
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
        SyncTestUtil.waitForSyncTransportActive();

        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                runOnUiThreadBlocking(
                        () -> {
                            return mBookmarkTestRule
                                    .getBookmarkActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_bookmark_and_other");
    }
}
