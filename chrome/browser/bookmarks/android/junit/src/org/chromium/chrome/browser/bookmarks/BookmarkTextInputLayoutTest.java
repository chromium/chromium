// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BookmarkTextInputLayout}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkTextInputLayoutTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BookmarkDelegate mBookmarkDelegate;
    @Mock private DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private SelectionDelegate mSelectionDelegate;
    @Mock private Runnable mOpenSearchUiRunnable;
    @Mock private Callback mOpenFolderCallback;
    @Mock private BookmarkId mBookmarkId;
    @Mock private BookmarkItem mBookmarkItem;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;

    private Context mContext;
    private BookmarkTextInputLayout mBookmarkTextInputLayout;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        View customView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.bookmark_add_new_folder_input_layout, null);
        mBookmarkTextInputLayout = customView.findViewById(R.id.folder_title);
    }

    @Test
    public void testValidate() {
        mBookmarkTextInputLayout.getEditText().setText("test");
        assertTrue(mBookmarkTextInputLayout.validate());
    }

    @Test
    public void testValidate_empty() {
        assertFalse(mBookmarkTextInputLayout.validate());
    }
}
