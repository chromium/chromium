// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View.OnClickListener;
import android.widget.ImageView.ScaleType;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkPopupViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkPopupViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkPopupView mView;
    @Mock private Drawable mDrawable;
    @Mock private Runnable mRemoveRunnable;
    @Mock private Runnable mDoneRunnable;
    @Mock private Runnable mFolderRowRunnable;
    @Mock private Runnable mCloseRunnable;
    @Mock private Callback<String> mCallback;

    @Captor private ArgumentCaptor<OnClickListener> mClickListenerCaptor;
    @Captor private ArgumentCaptor<TextWatcher> mTextWatcherCaptor;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(BookmarkPopupProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);
    }

    @Test
    public void testBindTextProperties() {
        mModel.set(BookmarkPopupProperties.HEADER_TEXT, "Bookmark added");
        verify(mView).setHeaderText("Bookmark added");

        mModel.set(BookmarkPopupProperties.TITLE, "Test Bookmark");
        verify(mView).setTitle("Test Bookmark");

        mModel.set(BookmarkPopupProperties.FOLDER_NAME, "Mobile Bookmarks");
        verify(mView).setFolderName("Mobile Bookmarks");
    }

    @Test
    public void testBindImageProperties() {
        mModel.set(BookmarkPopupProperties.IMAGE_DRAWABLE, mDrawable);
        verify(mView).setImageDrawable(mDrawable);

        mModel.set(BookmarkPopupProperties.IMAGE_SCALE_TYPE, ScaleType.CENTER);
        verify(mView).setImageScaleType(ScaleType.CENTER);

        mModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, true);
        verify(mView).setImageVisible(true);

        mModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, false);
        verify(mView).setImageVisible(false);
    }

    @Test
    public void testBindClickListeners() {
        mModel =
                new PropertyModel.Builder(BookmarkPopupProperties.ALL_KEYS)
                        .with(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, mRemoveRunnable)
                        .with(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, mCloseRunnable)
                        .with(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, mDoneRunnable)
                        .with(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, mFolderRowRunnable)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);

        verify(mView).setRemoveClickListener(mClickListenerCaptor.capture());
        mClickListenerCaptor.getValue().onClick(null);
        verify(mRemoveRunnable).run();

        verify(mView).setCloseClickListener(mClickListenerCaptor.capture());
        mClickListenerCaptor.getValue().onClick(null);
        verify(mCloseRunnable).run();

        verify(mView).setDoneClickListener(mClickListenerCaptor.capture());
        mClickListenerCaptor.getValue().onClick(null);
        verify(mDoneRunnable).run();

        verify(mView).setFolderRowClickListener(mClickListenerCaptor.capture());
        mClickListenerCaptor.getValue().onClick(null);
        verify(mFolderRowRunnable).run();
    }

    @Test
    public void testBindTitleChangedListener() {
        mModel =
                new PropertyModel.Builder(BookmarkPopupProperties.ALL_KEYS)
                        .with(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, mCallback)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);

        verify(mView).setTitleTextWatcher(mTextWatcherCaptor.capture());

        mTextWatcherCaptor.getValue().onTextChanged("Updated title", 0, 0, 13);
        verify(mCallback).onResult("Updated title");
    }

    @Test
    public void testNullListenersDoNotCrash() {
        mModel =
                new PropertyModel.Builder(BookmarkPopupProperties.ALL_KEYS)
                        .with(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, null)
                        .with(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, null)
                        .with(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, null)
                        .with(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, null)
                        .with(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, null)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);

        verify(mView).setRemoveClickListener(null);
        verify(mView).setCloseClickListener(null);
        verify(mView).setDoneClickListener(null);
        verify(mView).setFolderRowClickListener(null);
        verify(mView).setTitleTextWatcher(null);

        verifyNoInteractions(
                mRemoveRunnable, mCloseRunnable, mDoneRunnable, mFolderRowRunnable, mCallback);
    }
}
