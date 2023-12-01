// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipDescription;
import android.content.Context;
import android.view.DragEvent;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(org.chromium.base.test.BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID,
})
@DisableFeatures({
    ChromeFeatureList.TAB_DRAG_DROP_ANDROID,
})
public class ChromeTabbedOnDragListenerUnitTest {
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mCurrentTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplierImpl;
    private ClipDescription mClipDescription;
    private Context mContext;
    private ChromeTabbedOnDragListener mChromeTabbedOnDragListener;
    private View mCompositorViewHolder;
    private DragDropGlobalState mDragDropGlobalState;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = ContextUtils.getApplicationContext();
        mLayoutStateProviderSupplierImpl = new OneshotSupplierImpl<LayoutStateProvider>();
        mLayoutStateProviderSupplierImpl.set(mLayoutStateProvider);
        mChromeTabbedOnDragListener =
                new ChromeTabbedOnDragListener(
                        mMultiInstanceManager,
                        mTabModelSelector,
                        mWindowAndroid,
                        mLayoutStateProviderSupplierImpl);
        mCompositorViewHolder = new View(mContext);
        mDragDropGlobalState = DragDropGlobalState.getInstance();
        Activity activity = Mockito.mock(Activity.class);
        WeakReference weakActivity = new WeakReference(activity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);
    }

    @After
    public void tearDown() {
        mDragDropGlobalState.reset();
    }

    @Test
    public void testOnDrag_ActionDragStarted() {
        // Drag started should return false, since drag source is not chrome tab.
        mDragDropGlobalState.tabBeingDragged = Mockito.mock(Tab.class);
        mClipDescription = Mockito.spy(new ClipDescription(null, new String[] {"/image*"}));
        Assert.assertFalse(
                "Drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));

        // Drag started should return false, since dragged tab is null.
        mDragDropGlobalState.tabBeingDragged = null;
        mClipDescription = Mockito.spy(new ClipDescription(null, new String[] {"chrome/tab"}));
        Assert.assertFalse(
                "Drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));

        // Drag started should return true
        mDragDropGlobalState.tabBeingDragged = Mockito.mock(Tab.class);
        mClipDescription = Mockito.spy(new ClipDescription(null, new String[] {"chrome/tab"}));
        Assert.assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));
    }

    @Test
    public void testOnDrag_ActionDrop() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mCurrentTab);
        when(mCurrentTab.isIncognito()).thenReturn(false);
        when(mCurrentTab.getId()).thenReturn(1);
        when(mTabModelSelector.getModel(false)).thenReturn(Mockito.mock(TabModel.class));
        mDragDropGlobalState.dragSourceInstanceId = 1;

        // Drop should return false, since the destination instance is the same as the source
        // instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(1);
        Assert.assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));

        // Drop should return false, since it is trying to drop into tab switcher.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(1);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        Assert.assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));

        // Drop should return true, since the destination instance is not the same as the source
        // instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        Assert.assertTrue(
                "Action drop should return true",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));
    }

    @Test
    public void testOnDrag_ActionDragEnded() {
        mDragDropGlobalState.tabBeingDragged = Mockito.mock(Tab.class);

        //  DragDropGlobalState should not be cleared yet,
        Assert.assertNotEquals(
                "DragDropGlobalState should not be cleared",
                DragDropGlobalState.getInstance().tabBeingDragged,
                null);

        // DragDropGlobalState should be cleared.
        Assert.assertTrue(
                "Drag ended should return true",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DRAG_ENDED, true)));
        Assert.assertEquals(
                "DragDropGlobalState should be cleared",
                DragDropGlobalState.getInstance().tabBeingDragged,
                null);
    }

    private DragEvent mockDragEvent(int action, boolean result) {
        DragEvent event = Mockito.mock(DragEvent.class);
        when(event.getResult()).thenReturn(result);
        when(event.getClipDescription()).thenReturn(mClipDescription);
        doReturn(action).when(event).getAction();
        return event;
    }
}
