// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.messages.MessageBannerProperties.ON_FULLY_VISIBLE;
import static org.chromium.components.messages.MessageBannerProperties.ON_PRIMARY_ACTION;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VersioningMessageBanner}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VersioningMessageBannerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);
    }

    @Test
    public void testMaybeShow_offTheRecord() {
        when(mProfile.isOffTheRecord()).thenReturn(true);

        VersioningMessageBanner.maybeShow(
                mContext, mMessageDispatcher, mModalDialogManager, mProfile);

        verify(mVersioningMessageController, never()).shouldShowMessageUiAsync(anyInt(), any());
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void testMaybeShow_shouldNotShow() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        doCallback(1, (Callback<Boolean> callback) -> callback.onResult(false))
                .when(mVersioningMessageController)
                .shouldShowMessageUiAsync(
                        eq(MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE), any());

        VersioningMessageBanner.maybeShow(
                mContext, mMessageDispatcher, mModalDialogManager, mProfile);

        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());
    }

    @Test
    public void testMaybeShow_shouldShow() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        doCallback(1, (Callback<Boolean> callback) -> callback.onResult(true))
                .when(mVersioningMessageController)
                .shouldShowMessageUiAsync(
                        eq(MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE), any());
        VersioningMessageBanner.maybeShow(
                mContext, mMessageDispatcher, mModalDialogManager, mProfile);
        verify(mMessageDispatcher)
                .enqueueWindowScopedMessage(mPropertyModelCaptor.capture(), anyBoolean());

        PropertyModel model = mPropertyModelCaptor.getValue();
        model.get(ON_FULLY_VISIBLE).onResult(false);
        verify(mVersioningMessageController, never()).onMessageUiShown(anyInt());

        model.get(ON_FULLY_VISIBLE).onResult(true);
        verify(mVersioningMessageController)
                .onMessageUiShown(MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE);

        @PrimaryActionClickBehavior int behavior = model.get(ON_PRIMARY_ACTION).get();
        assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, behavior);
        verify(mModalDialogManager).showDialog(any(), anyInt());
    }
}
