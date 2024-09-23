// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipDescription;
import android.content.Context;
import android.view.DragEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropTabResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;

import java.lang.ref.WeakReference;

@RunWith(org.chromium.base.test.BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
public class ChromeTabbedOnDragListenerUnitTest {
    private static final int SOURCE_INSTANCE_ID = 1;
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mCurrentTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private DragDropGlobalState mDragDropGlobalState;
    @Mock private Tab mTab;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplierImpl;
    private ClipDescription mClipDescription =
            new ClipDescription(null, new String[] {"chrome/tab"});
    private Context mContext;
    private ChromeTabbedOnDragListener mChromeTabbedOnDragListener;
    private View mCompositorViewHolder;
    private UserActionTester mUserActionTest;

    @Before
    public void setup() {
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
        mUserActionTest = new UserActionTester();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mCurrentTab);
        when(mCurrentTab.isIncognito()).thenReturn(false);
        when(mCurrentTab.getId()).thenReturn(1);
        when(mTabModelSelector.getModel(false)).thenReturn(Mockito.mock(TabModel.class));
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(SOURCE_INSTANCE_ID);
        when(mDragDropGlobalState.getData())
                .thenReturn(new ChromeDropDataAndroid.Builder().withTab(mTab).build());
        when(mDragDropGlobalState.isDragSourceInstance(SOURCE_INSTANCE_ID)).thenReturn(true);
        DragDropGlobalState.setInstanceForTesting(mDragDropGlobalState);
        Activity activity = Mockito.mock(Activity.class);
        WeakReference weakActivity = new WeakReference(activity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);
    }

    @Test
    public void testOnDrag_ActionDragStarted() {
        // Drag started should return false, since drag source is not chrome tab.
        assertFalse(
                "Drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                false,
                                new ClipDescription(null, new String[] {"/image*"}))));
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));
    }

    @Test
    public void testOnDrag_ActionDragStarted_GlobalStateNotSet() {
        // Drag started should return false, since dragged state is not set.
        DragDropGlobalState.clearForTesting();
        assertFalse(
                "Drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));
    }

    @Test
    public void testOnDrag_ActionDrop_TabSwitcher() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_TAB_SWITCHER)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .build();
        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));

        // Drop should return false, since it is trying to drop into tab switcher.
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));
        histogramExpectation.assertExpected();
    }

    @Test
    public void testOnDrag_ActionDrop_SameInstance() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_SAME_INSTANCE)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .build();
        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));
        // Drop should return false, since the destination instance is the same as the source
        // instance.
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));
        histogramExpectation.assertExpected();
    }

    @Test
    public void testOnDrag_ActionDrop_Success() {
        // Verify action drop is success.
        verifyActionDropSuccess();

        // Verify user action `TabRemovedFromGroup` is not recorded.
        assertEquals(
                "TabRemovedFromGroup should not be recorded as the tab being dragged is not in a"
                        + " tab group",
                0,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    public void testOnDrag_ActionDrop_Success_RecordTabRemovedFromGroup() {
        // The tab being dragged is in a tab group.
        when(mDragDropGlobalState.getData())
                .thenReturn(
                        new ChromeDropDataAndroid.Builder()
                                .withTab(mTab)
                                .withTabInGroup(true)
                                .build());

        // Verify action drop is success.
        verifyActionDropSuccess();

        // Verify user action `TabRemovedFromGroup` is recorded.
        assertEquals(
                "TabRemovedFromGroup should be recorded",
                1,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    private void verifyActionDropSuccess() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.Type", DragDropType.TAB_STRIP_TO_CONTENT);
        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, false)));

        // Drop should return true, since the destination instance is not the same as the source
        // instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        assertTrue(
                "Action drop should return true",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder, mockDragEvent(DragEvent.ACTION_DROP, false)));
        histogramExpectation.assertExpected();
    }

    private DragEvent mockDragEvent(int action, boolean result) {
        return mockDragEvent(action, result, mClipDescription);
    }

    private DragEvent mockDragEvent(int action, boolean result, ClipDescription clipDescription) {
        DragEvent event = Mockito.mock(DragEvent.class);
        when(event.getResult()).thenReturn(result);
        when(event.getClipDescription()).thenReturn(clipDescription);
        doReturn(action).when(event).getAction();
        return event;
    }
}
