// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkToolbarViewBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BookmarkToolbar mBookmarkToolbar;

    private PropertyModel mModel;

    @Before
    public void before() {
        mModel =
                new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT, false)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB, false)
                        .with(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                false)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE, false)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, false)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ, false)
                        .with(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD, false)
                        .build();
    }

    @Test
    public void testBindSelectionModeShowCopyLink_true() {
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, true);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        verify(mBookmarkToolbar).setSelectionShowCopyLink(true);
    }

    @Test
    public void testBindSelectionModeShowCopyLink_false() {
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, false);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        verify(mBookmarkToolbar).setSelectionShowCopyLink(false);
    }

    @Test
    public void testBindSelectionModeShowCopyLink_change() {
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, true);
        verify(mBookmarkToolbar).setSelectionShowCopyLink(true);

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, false);
        verify(mBookmarkToolbar, times(2)).setSelectionShowCopyLink(false);
    }

    @Test
    public void testBindChromeIconVisible_true() {
        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, true);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        verify(mBookmarkToolbar).setChromeIconVisible(true);
    }

    @Test
    public void testBindChromeIconVisible_false() {
        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, false);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        verify(mBookmarkToolbar).setChromeIconVisible(false);
    }

    @Test
    public void testBindChromeIconVisible_change() {
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);

        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, true);
        verify(mBookmarkToolbar).setChromeIconVisible(true);

        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, false);
        verify(mBookmarkToolbar).setChromeIconVisible(false);
    }

    @Test
    public void testBindNavigationButtonState() {
        mModel.set(
                BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE,
                NavigationButton.NORMAL_VIEW_BACK);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        verify(mBookmarkToolbar).setNavigationButtonState(NavigationButton.NORMAL_VIEW_BACK);
    }
}
