// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.doNothing;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link MessageCardProviderMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class MessageCardProviderMediatorUnitTest {
    MessageCardProviderMediator mMediator;

    @Mock
    MessageCardView.DismissActionProvider mUiDismissActionProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doNothing().when(mUiDismissActionProvider).dismiss();
        mMediator = new MessageCardProviderMediator(mUiDismissActionProvider);
    }

    private void enqueueMessageItem(int type) {
        // TODO(crbug.com/1004570): Use MessageData instead of null when ready to integrate with
        //  MessageService component.
        mMediator.messageReady(type, null);
    }

    @Test
    public void getMessageItemsTest() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void getMessageItemsTest_TwoDifferentTypeMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertEquals(1, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());

        enqueueMessageItem(MessageService.MessageType.FOR_TESTING);

        Assert.assertEquals(2, mMediator.getMessageItems().size());
        Assert.assertTrue(mMediator.getReadyMessageItemsForTesting().isEmpty());
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().isEmpty());
    }

    @Test
    public void invalidate_queuedMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        mMediator.messageInvalidate(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertFalse(mMediator.getReadyMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
    }

    @Test
    public void invalidate_shownMessage() {
        enqueueMessageItem(MessageService.MessageType.TAB_SUGGESTION);

        mMediator.getMessageItems();
        mMediator.messageInvalidate(MessageService.MessageType.TAB_SUGGESTION);

        Assert.assertFalse(mMediator.getShownMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
        Assert.assertFalse(mMediator.getReadyMessageItemsForTesting().containsKey(
                MessageService.MessageType.TAB_SUGGESTION));
    }
}