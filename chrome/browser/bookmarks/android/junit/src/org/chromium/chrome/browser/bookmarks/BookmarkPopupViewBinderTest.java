// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkPopupViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkPopupViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mRemoveRunnable;
    @Mock private Runnable mDoneRunnable;
    @Mock private Runnable mFolderRowRunnable;
    @Mock private Runnable mCloseRunnable;
    @Mock private Callback<String> mCallback;

    private BookmarkPopupView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (BookmarkPopupView)
                        LayoutInflater.from(activity).inflate(R.layout.bookmark_popup, null);
        mModel = new PropertyModel(BookmarkPopupProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);
    }

    @Test
    public void testBindTextProperties() {
        mModel.set(BookmarkPopupProperties.HEADER_TEXT, "Bookmark added");
        assertEquals("Bookmark added", getText(R.id.popup_title));

        mModel.set(BookmarkPopupProperties.TITLE, "Test Bookmark");
        assertEquals("Test Bookmark", getText(R.id.bookmark_title));

        mModel.set(BookmarkPopupProperties.FOLDER_NAME, "Mobile Bookmarks");
        assertEquals("Mobile Bookmarks", getText(R.id.folder_title));
    }

    @Test
    public void testBindImageProperties() {
        ImageView imageView = mView.findViewById(R.id.bookmark_image);
        View imageContainer = mView.findViewById(R.id.bookmark_image_container);
        Drawable drawable = new ColorDrawable(Color.RED);

        mModel.set(BookmarkPopupProperties.IMAGE_DRAWABLE, drawable);
        assertEquals(drawable, imageView.getDrawable());

        mModel.set(BookmarkPopupProperties.IMAGE_SCALE_TYPE, ScaleType.CENTER);
        assertEquals(ScaleType.CENTER, imageView.getScaleType());

        mModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, true);
        assertEquals(View.VISIBLE, imageContainer.getVisibility());

        mModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, false);
        assertEquals(View.GONE, imageContainer.getVisibility());
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

        mView.findViewById(R.id.remove_button).performClick();
        verify(mRemoveRunnable).run();

        mView.findViewById(R.id.close_button).performClick();
        verify(mCloseRunnable).run();

        mView.findViewById(R.id.done_button).performClick();
        verify(mDoneRunnable).run();

        mView.findViewById(R.id.folder_picker_row).performClick();
        verify(mFolderRowRunnable).run();
    }

    @Test
    public void testBindTitleChangedListener() {
        mModel =
                new PropertyModel.Builder(BookmarkPopupProperties.ALL_KEYS)
                        .with(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, mCallback)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);

        EditText titleView = mView.findViewById(R.id.bookmark_title);
        titleView.setText("Updated title");
        verify(mCallback).onResult("Updated title");
    }

    @Test
    public void testNullListenersDoNotCrash() {
        mModel =
                new PropertyModel.Builder(BookmarkPopupProperties.ALL_KEYS)
                        .with(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, mRemoveRunnable)
                        .with(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, mCloseRunnable)
                        .with(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, mDoneRunnable)
                        .with(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, mFolderRowRunnable)
                        .with(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, mCallback)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, BookmarkPopupViewBinder::bind);

        // Clearing the listeners should detach them from the views.
        mModel.set(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, null);
        mModel.set(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, null);
        mModel.set(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, null);
        mModel.set(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, null);
        mModel.set(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, null);

        int[] clickableIds = {
            R.id.remove_button, R.id.close_button, R.id.done_button, R.id.folder_picker_row
        };
        for (int id : clickableIds) {
            View view = mView.findViewById(id);
            assertFalse(view.hasOnClickListeners());
            view.performClick();
        }
        ((EditText) mView.findViewById(R.id.bookmark_title)).setText("Updated title");

        verifyNoInteractions(
                mRemoveRunnable, mCloseRunnable, mDoneRunnable, mFolderRowRunnable, mCallback);
    }

    private String getText(int viewId) {
        return ((TextView) mView.findViewById(viewId)).getText().toString();
    }
}
