// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link BookmarkOpenerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkOpenerUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private BookmarkId mBookmarkId;

    private Activity mActivity;
    private BookmarkOpenerImpl mBookmarkOpener;
    private BookmarkItem mBookmarkItem;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        mBookmarkItem =
                new BookmarkItem(
                        mBookmarkId,
                        "Test Title",
                        new GURL("https://google.com"),
                        false,
                        null,
                        true,
                        false,
                        0,
                        false,
                        0,
                        false);
        when(mBookmarkModel.getBookmarkById(mBookmarkId)).thenReturn(mBookmarkItem);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mBookmarkOpener =
                new BookmarkOpenerImpl(
                        () -> mBookmarkModel,
                        mActivity,
                        new ComponentName(mActivity, mActivity.getClass()));
    }

    @Test
    @SmallTest
    public void testOpenBookmarkInCurrentTabTargetsSourceWindow() {
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(2);

        mBookmarkOpener.openBookmarkInCurrentTab(mBookmarkId, false);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(2, startedIntent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
    }

    @Test
    @SmallTest
    public void testOpenBookmarksInNewWindowRemovesTargetWindowExtra() {
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(2);

        List<BookmarkId> ids = new ArrayList<>();
        ids.add(mBookmarkId);
        mBookmarkOpener.openBookmarksInNewWindow(ids, false);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(-1, startedIntent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
    }
}
