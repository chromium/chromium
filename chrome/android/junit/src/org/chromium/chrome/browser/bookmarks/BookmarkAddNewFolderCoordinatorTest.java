// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.appcompat.widget.DialogTitle;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BookmarkAddNewFolderCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class BookmarkAddNewFolderCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BookmarkId mUserBokmarkId;
    @Mock private BookmarkId mRootFolderId;
    @Mock private BookmarkId mOtherFolderId;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        // Setup the bookmark model ids/items.
        doReturn(mRootFolderId).when(mBookmarkModel).getRootFolderId();
        doReturn(mOtherFolderId).when(mBookmarkModel).getOtherFolderId();

        mAddNewFolderCoordinator =
                new BookmarkAddNewFolderCoordinator(mContext, mModalDialogManager, mBookmarkModel);
    }

    @Test
    public void testAdd() {
        mAddNewFolderCoordinator.show(mUserBokmarkId);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        assertEquals("Add", model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals("Cancel", model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        DialogTitle title =
                model.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.dialog_title);
        assertEquals("Create new folder", title.getText());

        BookmarkTextInputLayout folderTitle =
                model.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.folder_title);
        folderTitle.getEditText().setText("user folder");

        ModalDialogProperties.Controller dialogController =
                model.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mBookmarkModel).addFolder(mUserBokmarkId, 0, "user folder");
    }

    @Test
    public void testAdd_rootFolder() {
        mAddNewFolderCoordinator.show(mRootFolderId);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        BookmarkTextInputLayout folderTitle =
                model.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.folder_title);
        folderTitle.getEditText().setText("user folder");

        ModalDialogProperties.Controller dialogController =
                model.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mBookmarkModel).addFolder(mOtherFolderId, 0, "user folder");
    }
}
