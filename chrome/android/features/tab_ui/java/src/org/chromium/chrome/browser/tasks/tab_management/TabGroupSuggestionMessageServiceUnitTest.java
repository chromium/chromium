// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentCaptor.captor;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageModelFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupSuggestionMessageService.StartMergeAnimation;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupSuggestionMessageService.TabGroupSuggestionMessageData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabGroupSuggestionMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupSuggestionMessageServiceUnitTest {
    private static final @TabId int TAB1_ID = 10;
    private static final @TabId int TAB2_ID = 20;
    private static final @TabId int TAB3_ID = 30;
    private static final @TabId int TAB4_ID = 40;
    private static final int TAB1_INDEX = 1;
    private static final int TAB2_INDEX = 2;
    private static final int TAB3_INDEX = 3;
    private static final int TAB4_INDEX = 4;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private Callback<@TabId Integer> mAddOnMessageAfterTabCallback;
    @Mock private Runnable mOnDismissMessageListener;
    @Mock private SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    @Mock private StartMergeAnimation mStartMergeAnimation;
    @Mock private SuggestionMetricsService mSuggestionMetricsService;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;

    @Captor private ArgumentCaptor<MessageModelFactory> mMessageDataCaptor;

    private TabGroupSuggestionMessageService mTabGroupSuggestionMessageService;

    @Before
    public void setUp() {
        SuggestionMetricsServiceFactory.setForTesting(mSuggestionMetricsService);

        ObservableSupplierImpl<TabGroupModelFilter> tabGroupModelFilterSupplier =
                new ObservableSupplierImpl<>(mTabGroupModelFilter);
        mTabGroupSuggestionMessageService =
                spy(
                        new TabGroupSuggestionMessageService(
                                mContext,
                                tabGroupModelFilterSupplier,
                                mAddOnMessageAfterTabCallback,
                                mStartMergeAnimation));

        doAnswer(
                        invocation -> {
                            Runnable onAnimationEnd = invocation.getArgument(2);
                            onAnimationEnd.run();
                            return null;
                        })
                .when(mStartMergeAnimation)
                .start(anyInt(), any(), any());

        when(mContext.getString(R.string.tab_group_suggestion_message, 2))
                .thenReturn("Group 2 tabs?");
        when(mContext.getString(R.string.tab_group_suggestion_message_action_text, 2))
                .thenReturn("Group tabs");
        when(mContext.getString(R.string.no_thanks)).thenReturn("No thanks");

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB3_ID)).thenReturn(mTab3);
        when(mTabModel.getTabById(TAB4_ID)).thenReturn(mTab4);
        when(mTabModel.getTabByIdChecked(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabByIdChecked(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabByIdChecked(TAB3_ID)).thenReturn(mTab3);
        when(mTabModel.getTabByIdChecked(TAB4_ID)).thenReturn(mTab4);
        when(mTabModel.indexOf(mTab1)).thenReturn(TAB1_INDEX);
        when(mTabModel.indexOf(mTab2)).thenReturn(TAB2_INDEX);
        when(mTabModel.indexOf(mTab3)).thenReturn(TAB3_INDEX);
        when(mTabModel.indexOf(mTab4)).thenReturn(TAB4_INDEX);

        when(mTab1.getProfile()).thenReturn(mProfile);
        when(mTab2.getProfile()).thenReturn(mProfile);
        when(mTab3.getProfile()).thenReturn(mProfile);
        when(mTab4.getProfile()).thenReturn(mProfile);
    }

    @Test
    public void testAddGroupMessageForTabs_success() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);

        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(any(MessageModelFactory.class));
        verify(mAddOnMessageAfterTabCallback).onResult(TAB2_ID);
    }

    @Test
    public void testAddGroupMessageForTabs_emptyList() {
        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                Collections.emptyList(), mSuggestionLifecycleObserver);

        verify(mTabGroupSuggestionMessageService, never())
                .sendAvailabilityNotification(any(MessageModelFactory.class));
        verify(mAddOnMessageAfterTabCallback, never()).onResult(any());
    }

    @Test
    public void testAddGroupMessageForTabs_alreadyShowing() {
        List<Integer> tabIds1 = List.of(TAB1_ID, TAB2_ID);
        List<Integer> tabIds2 = List.of(TAB3_ID, TAB4_ID);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds1, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(any(MessageModelFactory.class));
        verify(mAddOnMessageAfterTabCallback).onResult(TAB2_ID);

        reset(mAddOnMessageAfterTabCallback);
        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds2, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(any(MessageModelFactory.class));
        verify(mAddOnMessageAfterTabCallback, never()).onResult(any());
    }

    @Test
    public void testAddGroupMessageForTabs_outOfOrder() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB4_ID, TAB2_ID, TAB3_ID);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);

        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(any(MessageModelFactory.class));
        verify(mAddOnMessageAfterTabCallback).onResult(TAB3_ID);
    }

    @Test
    public void testDismissMessage_whenShowing() {
        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                List.of(TAB1_ID, TAB2_ID), mSuggestionLifecycleObserver);
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
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);
        List<Tab> tabs = List.of(mTab1, mTab2);
        Token tabGroupId = new Token(1L, 2L);
        when(mTab1.getTabGroupId()).thenReturn(tabGroupId);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        MessageModelFactory modelFactory = mMessageDataCaptor.getValue();
        PropertyModel model = modelFactory.build(mContext, ignored -> {});
        MessageCardView.ActionProvider reviewAction = model.get(UI_ACTION_PROVIDER);

        reviewAction.action();
        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        tabs,
                        mTab1,
                        TabGroupModelFilter.MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
        verify(mSuggestionMetricsService)
                .onSuggestionAccepted(
                        anyInt(), eq(GroupCreationSource.GTS_SUGGESTION), eq(tabGroupId));
    }

    @Test
    public void testGroupTabsAction_tabsNoLongerExist() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(null);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(null);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        MessageModelFactory modelFactory = mMessageDataCaptor.getValue();
        PropertyModel model = modelFactory.build(mContext, ignored -> {});
        MessageCardView.ActionProvider reviewAction = model.get(UI_ACTION_PROVIDER);
        reviewAction.action();

        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testDismissAction() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        MessageModelFactory modelFactory = mMessageDataCaptor.getValue();
        PropertyModel model = modelFactory.build(mContext, ignored -> {});
        MessageCardView.ActionProvider dismissAction = model.get(UI_DISMISS_ACTION_PROVIDER);

        dismissAction.action();
        verify(mSuggestionLifecycleObserver).onSuggestionDismissed();
        verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testAcceptCallbackBeforeDismiss() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);
        List<Tab> tabs = List.of(mTab1, mTab2);
        List<Integer> shiftedTabIds = List.of(TAB2_ID);

        // Reset mock to control callbacks manually.
        reset(mStartMergeAnimation);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        MessageModelFactory modelFactory = mMessageDataCaptor.getValue();
        PropertyModel model = modelFactory.build(mContext, ignored -> {});
        MessageCardView.ActionProvider reviewAction = model.get(UI_ACTION_PROVIDER);

        reviewAction.action();
        InOrder inOrder =
                inOrder(
                        mSuggestionLifecycleObserver,
                        mStartMergeAnimation,
                        mTabGroupModelFilter,
                        mTabGroupSuggestionMessageService);

        // Accept callback is called first.
        inOrder.verify(mSuggestionLifecycleObserver).onSuggestionAccepted();

        // Then animation starts.
        ArgumentCaptor<Runnable> onAnimationEndCaptor = captor();
        inOrder.verify(mStartMergeAnimation)
                .start(eq(TAB1_ID), eq(shiftedTabIds), onAnimationEndCaptor.capture());
        verify(mTabGroupSuggestionMessageService, never()).dismissMessage(any());
        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(any(), any(), anyInt());

        // Simulate tab group ID being set.
        doAnswer(
                        ignored -> {
                            when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());
                            return null;
                        })
                .when(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        tabs,
                        mTab1,
                        TabGroupModelFilter.MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);

        // Simulate animation end.
        onAnimationEndCaptor.getValue().run();

        // After animation, tabs are grouped and message is dismissed.
        inOrder.verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        tabs,
                        mTab1,
                        TabGroupModelFilter.MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP);
        inOrder.verify(mTabGroupSuggestionMessageService).dismissMessage(any());
    }

    @Test
    public void testMessageDataGetters() {
        int numTabs = 2;
        TabGroupSuggestionMessageData data =
                new TabGroupSuggestionMessageData(numTabs, mContext, () -> {}, () -> {});

        assertEquals("Group 2 tabs?", data.getMessageText());
        assertEquals("Group tabs", data.getActionText());
        assertEquals("No thanks", data.getDismissActionText());
    }

    @Test
    public void testIsIncognitoFalse() {
        List<Integer> tabIds = List.of(TAB1_ID, TAB2_ID);

        mTabGroupSuggestionMessageService.addGroupMessageForTabs(
                tabIds, mSuggestionLifecycleObserver);
        verify(mTabGroupSuggestionMessageService)
                .sendAvailabilityNotification(mMessageDataCaptor.capture());

        MessageModelFactory modelFactory = mMessageDataCaptor.getValue();
        PropertyModel model = modelFactory.build(mContext, ignored -> {});

        assertEquals(false, model.get(MessageCardViewProperties.IS_INCOGNITO));
    }
}
