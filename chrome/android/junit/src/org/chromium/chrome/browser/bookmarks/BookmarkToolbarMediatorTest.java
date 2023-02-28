// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkToolbarMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    BookmarkDelegate mBookmarkDelegate;
    @Mock
    BookmarkItemsAdapter mBookmarkItemsAdapter;

    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS)
                         .with(BookmarkToolbarProperties.DRAG_REORDERABLE_LIST_ADAPTER,
                                 mBookmarkItemsAdapter)
                         .with(BookmarkToolbarProperties.BOOKMARK_UI_STATE,
                                 BookmarkUIState.STATE_LOADING)
                         .build();

        mMediator = new BookmarkToolbarMediator(mModel);
    }

    @Test
    public void initSetsUpObservation() {
        mMediator.initialize(mBookmarkDelegate);

        Mockito.verify(mBookmarkDelegate).addUIObserver(mMediator);
    }

    @Test
    public void onStateChangedUpdatesModel() {
        mMediator.onStateChanged(BookmarkUIState.STATE_LOADING);
        Assert.assertEquals(BookmarkUIState.STATE_LOADING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_STATE));

        mMediator.onStateChanged(BookmarkUIState.STATE_SEARCHING);
        Assert.assertEquals(BookmarkUIState.STATE_SEARCHING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_STATE));

        mMediator.onStateChanged(BookmarkUIState.STATE_FOLDER);
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_STATE));
    }

    @Test
    public void destroyUnregistersObserver() {
        mMediator.initialize(mBookmarkDelegate);
        Mockito.verify(mBookmarkDelegate).addUIObserver(mMediator);

        mMediator.onDestroy();
        Mockito.verify(mBookmarkDelegate).removeUIObserver(mMediator);
    }
}