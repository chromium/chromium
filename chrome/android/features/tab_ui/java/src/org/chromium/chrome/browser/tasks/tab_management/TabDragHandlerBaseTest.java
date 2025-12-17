// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.DragEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.dragdrop.ChromeMultiTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDragStateData;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.ui.dragdrop.DragAndDropDelegate;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabDragHandlerBase}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabDragHandlerBaseTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private DragAndDropDelegate mDragAndDropDelegate;
    @Mock private View.DragShadowBuilder mDragShadowBuilder;

    private TabDragHandlerBase mTabDragHandler;

    @Before
    public void setUp() {
        mTabDragHandler =
                new TabDragHandlerBase(
                        () -> mActivity, mMultiInstanceManager, mDragAndDropDelegate, () -> false) {
                    @Override
                    public boolean onDrag(View v, DragEvent event) {
                        return false;
                    }
                };
    }

    private Tab createMockTab(int id) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.getTitle()).thenReturn("Tab " + id);
        when(tab.getProfile()).thenReturn(mProfile);
        return tab;
    }

    @Test
    public void testStartDrag_SingleTab() {
        Tab tab = createMockTab(1);
        var dropData = new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        when(mDragAndDropDelegate.startDragAndDrop(any(), any(), any())).thenReturn(true);

        mTabDragHandler.startDrag(mock(View.class), mDragShadowBuilder, dropData);
        assertTrue(TabDragStateData.getForTab(tab).getIsDraggingSupplier().get());

        mTabDragHandler.finishDrag(true);
        assertFalse(TabDragStateData.getForTab(tab).getIsDraggingSupplier().get());
    }

    @Test
    public void testStartDrag_MultiTab() {
        Tab tab1 = createMockTab(1);
        Tab tab2 = createMockTab(2);
        List<Tab> tabs = Arrays.asList(tab1, tab2);
        var dropData = new ChromeMultiTabDropDataAndroid.Builder().withTabs(tabs).build();
        when(mDragAndDropDelegate.startDragAndDrop(any(), any(), any())).thenReturn(true);

        mTabDragHandler.startDrag(mock(View.class), mDragShadowBuilder, dropData);
        assertTrue(TabDragStateData.getForTab(tab1).getIsDraggingSupplier().get());
        assertTrue(TabDragStateData.getForTab(tab2).getIsDraggingSupplier().get());

        mTabDragHandler.finishDrag(true);
        assertFalse(TabDragStateData.getForTab(tab1).getIsDraggingSupplier().get());
        assertFalse(TabDragStateData.getForTab(tab2).getIsDraggingSupplier().get());
    }

    @Test
    public void testStartDrag_TabGroup() {
        Tab tab1 = createMockTab(1);
        Tab tab2 = createMockTab(2);
        List<Tab> tabs = Arrays.asList(tab1, tab2);
        TabGroupMetadata tabGroupMetadata = mock(TabGroupMetadata.class);
        var dropData =
                new ChromeTabGroupDropDataAndroid.Builder()
                        .withTabGroupMetadata(tabGroupMetadata)
                        .withTabs(tabs)
                        .build();
        when(mDragAndDropDelegate.startDragAndDrop(any(), any(), any())).thenReturn(true);

        mTabDragHandler.startDrag(mock(View.class), mDragShadowBuilder, dropData);
        assertTrue(TabDragStateData.getForTab(tab1).getIsDraggingSupplier().get());
        assertTrue(TabDragStateData.getForTab(tab2).getIsDraggingSupplier().get());

        mTabDragHandler.finishDrag(true);
        assertFalse(TabDragStateData.getForTab(tab1).getIsDraggingSupplier().get());
        assertFalse(TabDragStateData.getForTab(tab2).getIsDraggingSupplier().get());
    }

    @Test
    public void testStartDrag_Fail() {
        Tab tab = createMockTab(1);
        var dropData = new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
        when(mDragAndDropDelegate.startDragAndDrop(any(), any(), any())).thenReturn(false);

        mTabDragHandler.startDrag(mock(View.class), mDragShadowBuilder, dropData);
        assertFalse(TabDragStateData.getOrCreateForTab(tab).getIsDraggingSupplier().get());
    }
}
