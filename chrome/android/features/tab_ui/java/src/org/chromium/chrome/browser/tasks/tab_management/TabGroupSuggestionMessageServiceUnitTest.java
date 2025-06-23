// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.TAB_GROUP_SUGGESTION_MESSAGE;

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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupSuggestionMessageService.TabGroupSuggestionMessageData;
import org.chromium.chrome.tab_ui.R;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabGroupSuggestionMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupSuggestionMessageServiceUnitTest {
    private static final @TabId int TAB1_ID = 1;
    private static final @TabId int TAB2_ID = 2;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Runnable mOnAddMessageListener;
    @Mock private Runnable mOnDismissMessageListener;
    @Mock private SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    @Captor private ArgumentCaptor<MessageData> mMessageDataCaptor;

    private TabGroupSuggestionMessageService mTabGroupSuggestionMessageService;

    @Before
    public void setUp() {
        ObservableSupplierImpl<TabGroupModelFilter> tabGroupModelFilterSupplier =
                new ObservableSupplierImpl<>(mTabGroupModelFilter);
        mTabGroupSuggestionMessageService =
                spy(
                        new TabGroupSuggestionMessageService(
                                mContext, tabGroupModelFilterSupplier, mOnAddMessageListener));

        when(mContext.getString(R.string.tab_group_suggestion_message, 2))
                .thenReturn("Group 2 tabs?");
        when(mContext.getString(R.string.tab_group_suggestion_message_action_text, 2))
                .thenReturn("Group tabs");
        when(mContext.getString(R.string.no_thanks)).thenReturn("No thanks");

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(mTab2);
    }

    @Test
    public void testAddGroupMessageForTabs_success() {
        List<Integer> tabIds = List.of(1, 2);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);

        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(any(MessageData.class));
        verify(mOnAddMessageListener).run();
    }

    @Test
    public void testAddGroupMessageForTabs_emptyList() {
        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                Collections.emptyList(), mSuggestionLifecycleObserver);

        verify(mTabGroupSuggestionMessageService, never())
                .sendAvailabilityNotification(any(MessageData.class));
        verify(mOnAddMessageListener, never()).run();
    }

    @Test
    public void testAddGroupMessageForTabs_alreadyShowing() {
        List<Integer> tabIds1 = List.of(1, 2);
        List<Integer> tabIds2 = List.of(3, 4);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds1, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService, times(1))
                .sendAvailabilityNotification(any(MessageData.class));
        verify(mOnAddMessageListener, times(1)).run();

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds2, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService, times(1))
                .sendAvailabilityNotification(any(MessageData.class));
        verify(mOnAddMessageListener, times(1)).run();
    }

    @Test
    public void testDismissMessage_whenShowing() {
        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                List.of(1, 2), mSuggestionLifecycleObserver);
        mTabGroupSuggestionMessageService.dismissMessage(mOnDismissMessageListener);

        verify(mTabGroupSuggestionMessageService).sendInvalidNotification();
        verify(mOnDismissMessageListener).run();
    }

    @Test
    public void testDismissMessage_whenNotShowing() {
        mTabGroupSuggestionMessageService.dismissMessage(mOnDismissMessageListener);

        verify(mTabGroupSuggestionMessageService, never()).sendInvalidNotification();
        verify(mOnDismissMessageListener, never()).run();
    }

    @Test
    public void testGroupTabsAction() {
        List<Integer> tabIds = List.of(1, 2);
        List<Tab> tabs = List.of(mTab1, mTab2);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        TabGroupSuggestionMessageData data =
                (TabGroupSuggestionMessageData) mMessageDataCaptor.getValue();
        MessageCardView.ReviewActionProvider reviewAction = data.getReviewActionProvider();

        reviewAction.review();
        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(tabs, mTab1, true);
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testGroupTabsAction_tabsNoLongerExist() {
        List<Integer> tabIds = List.of(1, 2);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(null);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(null);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        TabGroupSuggestionMessageData data =
                (TabGroupSuggestionMessageData) mMessageDataCaptor.getValue();
        MessageCardView.ReviewActionProvider reviewAction = data.getReviewActionProvider();

        reviewAction.review();

        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyBoolean());
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testDismissAction() {
        List<Integer> tabIds = List.of(1, 2);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        TabGroupSuggestionMessageData data =
                (TabGroupSuggestionMessageData) mMessageDataCaptor.getValue();
        MessageCardView.DismissActionProvider dismissAction = data.getDismissActionProvider();

        dismissAction.dismiss(TAB_GROUP_SUGGESTION_MESSAGE);
        verify(mSuggestionLifecycleObserver).onSuggestionDismissed();
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testMessageDataGetters() {
        int numTabs = 2;
        TabGroupSuggestionMessageData data =
                new TabGroupSuggestionMessageData(numTabs, mContext, () -> {}, (reason) -> {});

        assertEquals("Group 2 tabs?", data.getMessageText());
        assertEquals("Group tabs", data.getActionText());
        assertEquals("No thanks", data.getDismissActionText());
    }
}
