// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Unit tests for {@link BookmarkToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkToolbarViewBinderTest {
    private static final BookmarkId BOOKMARK_ID = new BookmarkId(2, BookmarkType.NORMAL);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<BookmarkId> mSelectionDelegate;

    private BookmarkToolbar mBookmarkToolbar;
    private PropertyModel mModel;

    @Before
    public void before() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        LinearLayout contentView = new LinearLayout(activity);
        activity.setContentView(contentView);
        mBookmarkToolbar =
                activity.getLayoutInflater()
                        .inflate(R.layout.bookmark_toolbar, contentView, true)
                        .findViewById(R.id.bookmark_toolbar);
        mBookmarkToolbar.initialize(
                mSelectionDelegate,
                /* titleResId= */ 0,
                R.id.normal_menu_group,
                R.id.selection_mode_menu_group,
                /* updateStatusBarColor= */ false);

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

    /** Selection mode menu items may only be shown while in selection mode. */
    private void enterSelectionMode() {
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID));
    }

    private boolean isCopyLinkVisible() {
        return mBookmarkToolbar.getMenu().findItem(R.id.selection_mode_copy_link).isVisible();
    }

    @Test
    public void testBindSelectionModeShowCopyLink_true() {
        enterSelectionMode();
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, true);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertTrue(isCopyLinkVisible());
    }

    @Test
    public void testBindSelectionModeShowCopyLink_false() {
        enterSelectionMode();
        mBookmarkToolbar.setSelectionShowCopyLink(true);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, false);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertFalse(isCopyLinkVisible());
    }

    @Test
    public void testBindSelectionModeShowCopyLink_change() {
        enterSelectionMode();
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertFalse(isCopyLinkVisible());

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, true);
        assertTrue(isCopyLinkVisible());

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, false);
        assertFalse(isCopyLinkVisible());
    }

    @Test
    public void testBindChromeIconVisible_true() {
        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, true);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertNotNull(mBookmarkToolbar.getNavigationIcon());
    }

    @Test
    public void testBindChromeIconVisible_false() {
        mBookmarkToolbar.setChromeIconVisible(true);
        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, false);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertNull(mBookmarkToolbar.getNavigationIcon());
    }

    @Test
    public void testBindChromeIconVisible_change() {
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);

        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, true);
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        mModel.set(BookmarkToolbarProperties.CHROME_ICON_VISIBLE, false);
        assertNull(mBookmarkToolbar.getNavigationIcon());
    }

    @Test
    public void testBindNavigationButtonState() {
        mModel.set(
                BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE,
                NavigationButton.NORMAL_VIEW_BACK);
        PropertyModelChangeProcessor.create(
                mModel, mBookmarkToolbar, BookmarkToolbarViewBinder::bind);
        assertEquals(
                NavigationButton.NORMAL_VIEW_BACK, mBookmarkToolbar.getNavigationButtonForTests());
    }
}
