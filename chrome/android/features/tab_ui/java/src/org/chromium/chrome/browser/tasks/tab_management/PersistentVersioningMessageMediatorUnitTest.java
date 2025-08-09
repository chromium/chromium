// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType.VERSION_OUT_OF_DATE;

import android.content.Context;

import androidx.appcompat.view.ContextThemeWrapper;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.versioning.VersioningModalDialog;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ActionProvider;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PersistentVersioningMessageMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PersistentVersioningMessageMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private VersioningModalDialog mVersioningModalDialogMock;

    private Context mContext;
    private ModelList mModelList;
    private PersistentVersioningMessageMediator mMediator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mModelList = new ModelList();

        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);

        mMediator =
                new PersistentVersioningMessageMediator(
                        mContext,
                        mVersioningMessageController,
                        mModelList,
                        mVersioningModalDialogMock);
    }

    @Test(expected = AssertionError.class)
    public void testBuild_otrProfile() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        PersistentVersioningMessageMediator.build(
                mContext, mProfile, mModelList, mModalDialogManager);
    }

    @Test
    public void testBuild() {
        assertNotNull(
                PersistentVersioningMessageMediator.build(
                        mContext, mProfile, mModelList, mModalDialogManager));
    }

    @Test
    public void testQueueMessageIfNeeded() {
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE))
                .thenReturn(true);

        mMediator.queueMessageIfNeeded();

        assertEquals(1, mModelList.size());
        verify(mVersioningMessageController)
                .onMessageUiShown(MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    @Test
    public void testQueueMessageIfNeeded_notInitialized() {
        when(mVersioningMessageController.isInitialized()).thenReturn(false);
        mMediator.queueMessageIfNeeded();
        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testQueueMessageIfNeeded_shouldNotShowUi() {
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE))
                .thenReturn(false);

        mMediator.queueMessageIfNeeded();
        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testQueueMessageCardIfNeeded_alreadyExists() {
        mModelList.add(
                new ListItem(
                        VERSION_OUT_OF_DATE,
                        new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                .with(MessageCardViewProperties.MESSAGE_TYPE, VERSION_OUT_OF_DATE)
                                .build()));
        assertEquals(1, mModelList.size());
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE))
                .thenReturn(true);

        mMediator.queueMessageIfNeeded();
        assertEquals(1, mModelList.size());
        mMediator.removeMessageCard();
        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testOnPrimaryAction() {
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE))
                .thenReturn(true);
        mMediator.queueMessageIfNeeded();
        ListItem listItem = mModelList.get(0);
        PropertyModel model = listItem.model;
        ActionProvider actionProvider = model.get(MessageCardViewProperties.UI_ACTION_PROVIDER);

        actionProvider.action();
        verify(mVersioningModalDialogMock).show();
    }

    @Test
    public void testOnDismiss() {
        when(mVersioningMessageController.isInitialized()).thenReturn(true);
        when(mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE))
                .thenReturn(true);
        mMediator.queueMessageIfNeeded();
        assertEquals(1, mModelList.size());
        ListItem listItem = mModelList.get(0);
        PropertyModel model = listItem.model;
        MessageCardView.ActionProvider dismissProvider =
                model.get(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER);

        dismissProvider.action();

        assertTrue(mModelList.isEmpty());
        verify(mVersioningMessageController)
                .onMessageUiDismissed(MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    @Test
    public void testRemoveMessageCard() {
        mModelList.add(
                new ListItem(
                        VERSION_OUT_OF_DATE,
                        new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                .with(MessageCardViewProperties.MESSAGE_TYPE, VERSION_OUT_OF_DATE)
                                .build()));
        assertEquals(1, mModelList.size());

        mMediator.removeMessageCard();
        assertTrue(mModelList.isEmpty());
    }
}
