// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkAllTabsHandler.BookmarkAllTabsResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.Collections;

@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkAllTabsHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private BookmarkModel mBookmarkModel;

    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;

    private Activity mActivity;
    private BookmarkAllTabsHandler mHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        when(mBookmarkModel.getDefaultBookmarkFolder())
                .thenReturn(new BookmarkId(1, BookmarkType.NORMAL));

        BookmarkModel.setInstanceForTesting(mBookmarkModel);

        mHandler =
                new BookmarkAllTabsHandler(
                        () -> mTabModelSelector, () -> mSnackbarManager, mWindowAndroid);
    }

    @Test
    public void testHandleMenuOrKeyboardAction_WrongId() {
        assertFalse(mHandler.handleMenuOrKeyboardAction(-1, false));
    }

    @Test
    public void testHandleMenuOrKeyboardAction_NullSelector() {
        BookmarkAllTabsHandler handler =
                new BookmarkAllTabsHandler(() -> null, () -> mSnackbarManager, mWindowAndroid);
        // Should not crash, just return true as the event was consumed.
        assertTrue(handler.handleMenuOrKeyboardAction(R.id.bookmark_all_tabs, false));
    }

    @Test
    public void testBookmarkAllTabs_NullModel() {
        UserActionTester userActionTester = new UserActionTester();
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BookmarkAllTabs.Result", BookmarkAllTabsResult.MODEL_NULL);

        BookmarkAllTabsHandler.bookmarkAllTabs(null, mWindowAndroid, mSnackbarManager);

        watcher.assertExpected();
        assertTrue(userActionTester.getActions().contains("Android.BookmarkAllTabs"));
    }

    @Test
    public void testBookmarkAllTabs_EmptyTabList() {
        when(mTabModel.getCount()).thenReturn(0);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.emptyIterator());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BookmarkAllTabs.Result", BookmarkAllTabsResult.TAB_LIST_EMPTY);

        BookmarkAllTabsHandler.bookmarkAllTabs(mTabModel, mWindowAndroid, mSnackbarManager);

        watcher.assertExpected();
    }

    @Test
    public void testBookmarkAllTabs_Success() {
        GURL url = JUnitTestGURLs.EXAMPLE_URL;

        Tab mockTab = mock(Tab.class);
        doReturn(url).when(mockTab).getOriginalUrl();
        doReturn(url).when(mockTab).getUrl();

        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mockTab);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mockTab).iterator());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BookmarkAllTabs.Result", BookmarkAllTabsResult.SUCCESS);

        BookmarkAllTabsHandler.bookmarkAllTabs(mTabModel, mWindowAndroid, mSnackbarManager);

        verify(mBookmarkModel).finishLoadingBookmarkModel(mRunnableCaptor.capture());
        mRunnableCaptor.getValue().run();
        watcher.assertExpected();
    }
}
