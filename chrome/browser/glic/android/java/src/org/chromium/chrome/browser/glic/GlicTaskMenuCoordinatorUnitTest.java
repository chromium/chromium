// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
public class GlicTaskMenuCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private GlicButtonDelegate mToggleGlicCallback;

    private Context mContext;
    private GlicTaskMenuCoordinator mCoordinator;
    private List<ActorTask> mTasks;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mCoordinator =
                new GlicTaskMenuCoordinator(mContext, () -> mTabModelSelector, mToggleGlicCallback);

        ActorTask task1 = mock(ActorTask.class);
        doReturn("Task One").when(task1).getTitle();
        ActorTask task2 = mock(ActorTask.class);
        doReturn("Task Two").when(task2).getTitle();
        mTasks = Arrays.asList(task1, task2);
    }

    @Test
    public void testBuildModelList_WithActiveTasks() {
        ModelList modelList = mCoordinator.buildModelList(mTasks);

        // 2 tasks + 1 divider + 1 Ask Gemini = 4 items total
        assertEquals(4, modelList.size());

        ListItem item1 = modelList.get(0);
        assertEquals("Task One", item1.model.get(ListMenuItemProperties.TITLE));

        ListItem item2 = modelList.get(1);
        assertEquals("Task Two", item2.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testBuildModelList_SidePanelEnabled() {
        ModelList modelList = mCoordinator.buildModelList(mTasks);

        // 2 tasks = 2 items total
        assertEquals(2, modelList.size());

        ListItem item1 = modelList.get(0);
        assertEquals("Task One", item1.model.get(ListMenuItemProperties.TITLE));

        ListItem item2 = modelList.get(1);
        assertEquals("Task Two", item2.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    public void testClickAskGemini_TriggersCallbackWithFalse() {
        ModelList modelList = mCoordinator.buildModelList(Collections.emptyList());
        // Index 0 is divider, Index 1 is Ask Gemini
        ListItem askGeminiItem = modelList.get(1);

        View.OnClickListener clickListener =
                askGeminiItem.model.get(ListMenuItemProperties.CLICK_LISTENER);
        clickListener.onClick(null);

        verify(mToggleGlicCallback).onClick(/* preventClose= */ false);
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

        verify(mToggleGlicCallback).onClick(/* preventClose= */ true);
    }
}
