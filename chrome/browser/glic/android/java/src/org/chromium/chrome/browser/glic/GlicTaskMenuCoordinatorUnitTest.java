// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link GlicTaskMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicTaskMenuCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private GlicButtonDelegate mToggleGlicCallback;

    private Context mContext;
    private GlicTaskMenuCoordinator mCoordinator;
    private List<ActorTask> mTasks;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new GlicTaskMenuCoordinator(
                        mContext,
                        () -> mTabModelSelector,
                        mToggleGlicCallback,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON,
                        GlicTaskMenuCoordinator.ButtonSource.TOOLBAR);

        ActorTask task1 = mock(ActorTask.class);
        doReturn("Task One").when(task1).getTitle();
        ActorTask task2 = mock(ActorTask.class);
        doReturn("Task Two").when(task2).getTitle();
        mTasks = Arrays.asList(task1, task2);
    }

    @Test
    public void testBuildModelList_WithActiveTasks() {
        ModelList modelList = mCoordinator.buildModelList(mTasks);

        // 2 tasks + 1 divider + 1 Open Chat = 4 items total
        assertEquals(4, modelList.size());

        ListItem item1 = modelList.get(0);
        assertEquals("Task One", item1.model.get(ListMenuItemProperties.TITLE));
        assertNull(item1.model.get(ListMenuItemProperties.SUBTITLE));
        assertEquals(0, item1.model.get(ListMenuItemProperties.VERTICAL_PADDING));
        assertEquals(
                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle(),
                item1.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                Resources.ID_NULL,
                item1.model.get(ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID));

        ListItem item2 = modelList.get(1);
        assertEquals("Task Two", item2.model.get(ListMenuItemProperties.TITLE));
        assertNull(item2.model.get(ListMenuItemProperties.SUBTITLE));
    }

    @Test
    public void testBuildModelList_TabStripSource() {
        ActorTask task1 = mock(ActorTask.class);
        doReturn("Task One").when(task1).getTitle();
        doReturn(true).when(task1).isCompleted();
        ActorTask task2 = mock(ActorTask.class);
        doReturn("Task Two").when(task2).getTitle();
        doReturn(true).when(task2).isCompleted();

        GlicTaskMenuCoordinator tabStripCoordinator =
                new GlicTaskMenuCoordinator(
                        mContext,
                        () -> mTabModelSelector,
                        mToggleGlicCallback,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON,
                        GlicTaskMenuCoordinator.ButtonSource.TAB_STRIP);
        ModelList modelList = tabStripCoordinator.buildModelList(Arrays.asList(task1, task2));

        // 2 tasks = 2 items total (Open Chat hidden)
        assertEquals(2, modelList.size());

        ListItem item1 = modelList.get(0);
        assertEquals("Task One", item1.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                mContext.getString(R.string.actor_task_list_bubble_row_tab_closed_subtitle),
                item1.model.get(ListMenuItemProperties.SUBTITLE));
        assertEquals(
                R.drawable.glic_menu_end_icon_standard,
                item1.model.get(ListMenuItemProperties.END_ICON_ID));
        int expectedTabStripVerticalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.glic_task_menu_item_vertical_padding);
        assertEquals(
                expectedTabStripVerticalPadding,
                item1.model.get(ListMenuItemProperties.VERTICAL_PADDING));
        assertEquals(
                R.style.TextAppearance_TextLarge,
                item1.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                R.style.TextAppearance_TextMedium,
                item1.model.get(ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID));

        ListItem item2 = modelList.get(1);
        assertEquals("Task Two", item2.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                mContext.getString(R.string.actor_task_list_bubble_row_tab_closed_subtitle),
                item2.model.get(ListMenuItemProperties.SUBTITLE));
        assertEquals(
                R.drawable.glic_menu_end_icon_standard,
                item2.model.get(ListMenuItemProperties.END_ICON_ID));
    }

    @Test
    public void testBuildModelList_TabStripSource_NeedsReview() {
        ActorTask reviewTask = mock(ActorTask.class);
        doReturn("Review Task").when(reviewTask).getTitle();
        doReturn(ActorTaskState.WAITING_ON_USER).when(reviewTask).getState();

        GlicTaskMenuCoordinator tabStripCoordinator =
                new GlicTaskMenuCoordinator(
                        mContext,
                        () -> mTabModelSelector,
                        mToggleGlicCallback,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON,
                        GlicTaskMenuCoordinator.ButtonSource.TAB_STRIP);
        ModelList modelList =
                tabStripCoordinator.buildModelList(Collections.singletonList(reviewTask));

        assertEquals(1, modelList.size());
        ListItem item = modelList.get(0);
        assertEquals("Review Task", item.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.drawable.glic_menu_end_icon_needs_review,
                item.model.get(ListMenuItemProperties.END_ICON_ID));
    }

    @Test
    public void testClickOpenChat_TriggersCallbackWithFalse() {
        ModelList modelList = mCoordinator.buildModelList(Collections.emptyList());
        // Index 0 is divider, Index 1 is Open Chat
        ListItem openChatItem = modelList.get(1);

        View.OnClickListener clickListener =
                openChatItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mToggleGlicCallback)
                .onClick(
                        /* preventClose= */ false,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON);
    }

    @Test
    public void testClickActorTask_TriggersTabSwitchAndCallbackWithTrue() {
        ActorTask task = mock(ActorTask.class);
        doReturn("Task Title").when(task).getTitle();
        Set<Integer> tabIds = new HashSet<>(Arrays.asList(123));
        doReturn(tabIds).when(task).getLastActedTabs();

        ModelList modelList = mCoordinator.buildModelList(Arrays.asList(task));
        ListItem taskItem = modelList.get(0);

        View.OnClickListener clickListener =
                taskItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mToggleGlicCallback)
                .onClick(
                        /* preventClose= */ true,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON);
    }

    @Test
    public void testClickActorTask_EmptyTabs_DoesNotOpenNewTab() {
        ActorTask task = mock(ActorTask.class);
        doReturn("Task Title").when(task).getTitle();
        doReturn(Collections.emptySet()).when(task).getLastActedTabs();

        ModelList modelList = mCoordinator.buildModelList(Arrays.asList(task));
        ListItem taskItem = modelList.get(0);

        View.OnClickListener clickListener =
                taskItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mToggleGlicCallback)
                .onClick(
                        /* preventClose= */ true,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON);
    }

    @Test
    public void testClickActorTask_CompletedTaskClosedTab_OpensNewTab() {
        ActorTask task = mock(ActorTask.class);
        doReturn("Task Title").when(task).getTitle();
        doReturn(Collections.emptySet()).when(task).getLastActedTabs();
        doReturn(true).when(task).isCompleted();

        ModelList modelList = mCoordinator.buildModelList(Arrays.asList(task));
        ListItem taskItem = modelList.get(0);

        View.OnClickListener clickListener =
                taskItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mTabModelSelector)
                .openNewTab(any(), eq(TabLaunchType.FROM_CHROME_UI), eq(null), eq(false));
    }

    @Test
    public void testClickOpenChat_UsesConfiguredInvocationSource() {
        GlicTaskMenuCoordinator coordinator =
                new GlicTaskMenuCoordinator(
                        mContext,
                        () -> mTabModelSelector,
                        mToggleGlicCallback,
                        GlicKeyedService.GlicInvocationSource.TOOLBAR_BUTTON,
                        GlicTaskMenuCoordinator.ButtonSource.TOOLBAR);
        ModelList modelList = coordinator.buildModelList(Collections.emptyList());
        ListItem openChatItem = modelList.get(1);

        View.OnClickListener clickListener =
                openChatItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mToggleGlicCallback)
                .onClick(
                        /* preventClose= */ false,
                        GlicKeyedService.GlicInvocationSource.TOOLBAR_BUTTON);
    }
}
