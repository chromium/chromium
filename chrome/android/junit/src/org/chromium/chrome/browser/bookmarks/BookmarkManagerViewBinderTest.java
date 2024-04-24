// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.ItemTouchHelper;
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
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkManagerViewBinder}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkManagerViewBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock View mView;
    @Mock TextView mTextView;
    @Mock LinearLayout mLinearLayout;
    @Mock PersonalizedSigninPromoView mPromoView;
    @Mock Callback<BookmarkId> mOpenFolderCallback;
    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock Runnable mClearHighlightCallback;
    @Mock BookmarkPromoHeader mBookmarkPromoHeader;

    private Activity mActivity;
    private PropertyModel mModel;

    @Before
    public void before() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
    }

    @Test
    public void testConstructor() {
        new BookmarkManagerViewBinder();
    }

    @Test
    public void testBindPersonalizedPromoView() {
        when(mView.findViewById(R.id.signin_promo_view_container)).thenReturn(mPromoView);
        mModel.set(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER, mBookmarkPromoHeader);
        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindPersonalizedPromoView);
        verify(mBookmarkPromoHeader).setUpSyncPromoView(mPromoView);
    }

    @Test
    public void testBindLegacyPromoView() {
        mModel.set(BookmarkManagerProperties.BOOKMARK_ID, null);
        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindLegacyPromoView);
    }

    @Test
    public void testBindSectionHeaderView() {
        when(mView.getResources()).thenReturn(mActivity.getResources());
        when(mView.findViewById(anyInt())).thenReturn(mTextView);

        BookmarkListEntry bookmarkListEntry =
                BookmarkListEntry.createSectionHeader(
                        R.string.reading_list_read,
                        R.dimen.bookmark_reading_list_section_header_padding_top);
        mModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);

        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindSectionHeaderView);

        verify(mTextView).setText(mActivity.getResources().getString(R.string.reading_list_read));
        int expectedTopPadding =
                (int)
                        mActivity
                                .getResources()
                                .getDimension(
                                        R.dimen.bookmark_reading_list_section_header_padding_top);
        verify(mTextView).setPaddingRelative(anyInt(), eq(expectedTopPadding), anyInt(), anyInt());
    }

    @Test
    public void testBindDividerView() {
        mModel.set(BookmarkManagerProperties.BOOKMARK_ID, null);
        PropertyModelChangeProcessor.create(
                mModel, mView, BookmarkManagerViewBinder::bindDividerView);
    }
}
