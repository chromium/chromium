// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BookmarkManagerOpener}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkManagerOpenerTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private BookmarkFolderPickerActivity mBookmarkFolderPickerActivity;
    @Mock private BookmarkFolderPickerActivity mBookmarkFolderPickerActivity2;
    @Mock private Runnable mRunnable;

    private Activity mActivity;
    private final BookmarkManagerOpener mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @Test
    @SmallTest
    public void testReopeningBookmarkManagerRecordsMetric() {
        BookmarkUtils.setLastUsedUrl("https://test.com");
        UserActionTester userActionTester = new UserActionTester();

        mBookmarkManagerOpener.showBookmarkManager(mActivity, mProfile, /* folderId= */ null);
        assertEquals(
                1,
                userActionTester.getActionCount(
                        "MobileBookmarkManagerReopenBookmarksInSameSession"));
    }
}
