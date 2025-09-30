// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
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
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

@RunWith(org.chromium.base.test.BaseRobolectricTestRunner.class)
public class ChromeTabbedOnDragListenerUnitTest {
    private static final int SOURCE_INSTANCE_ID = 1;
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private Tab mCurrentTab;
    @Mock private NewTabPage mOriginalNtp;
    @Mock private NewTabPage mCurrentNtp;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private DragDropGlobalState mDragDropGlobalState;
    @Mock private TabGroupMetadata mTabGroupMetadata;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplierImpl;
    private final ClipDescription mTabClipDescription =
            new ClipDescription(null, new String[] {"chrome/tab"});
    private final ClipDescription mTabGroupClipDescription =
            new ClipDescription(null, new String[] {"chrome/tab-group"});
    private final ClipDescription mMultiTabClipDescription =
            new ClipDescription(null, new String[] {"chrome/multi-tab"});
    private Context mContext;
    private ChromeTabbedOnDragListener mChromeTabbedOnDragListener;
    private View mCompositorViewHolder;
    private UserActionTester mUserActionTest;

    @Before
    public void setup() {
        mContext = ContextUtils.getApplicationContext();
        mLayoutStateProviderSupplierImpl = new OneshotSupplierImpl<>();
        mLayoutStateProviderSupplierImpl.set(mLayoutStateProvider);
        mChromeTabbedOnDragListener =
                new ChromeTabbedOnDragListener(
                        mMultiInstanceManager,
                        mTabModelSelector,
                        mWindowAndroid,
                        mLayoutStateProviderSupplierImpl,
                        mDesktopWindowStateManager);
        mCompositorViewHolder = new View(mContext);
        mUserActionTest = new UserActionTester();
        when(mTabModelSelector.getCurrentTab()).thenReturn(mCurrentTab);
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);
        when(mCurrentTab.getId()).thenReturn(1);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTabModel.iterator()).thenAnswer(invocation -> List.of(mTab, mCurrentTab).iterator());
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(SOURCE_INSTANCE_ID);
        when(mDragDropGlobalState.isDragSourceInstance(SOURCE_INSTANCE_ID)).thenReturn(true);
        DragDropGlobalState.setInstanceForTesting(mDragDropGlobalState);
        Activity activity = Robolectric.setupActivity(Activity.class);
        WeakReference weakActivity = new WeakReference(activity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);
    }

    @Test
    public void testOnDrag_ActionDragStarted() {
        doTestOnDragActionDragStarted(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDragStarted_TabGroup() {
        doTestOnDragActionDragStarted(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDragStarted_MultiTab() {
        doTestOnDragActionDragStarted(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);
    }

    private void doTestOnDragActionDragStarted(boolean isGroupDrag, boolean isMultiTabDrag) {
        // Drag started should return false, since drag source is not chrome tab or group.
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
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
    }

    @Test
    public void testOnDrag_ActionDragStarted_GlobalStateNotSet() {
        // Drag started should return false, since dragged state is not set.
        DragDropGlobalState.clearForTesting();
        assertFalse(
                "Tab drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                /* isGroupDrag= */ false,
                                /* isMultiTabDrag= */ false)));
        assertFalse(
                "Tab group drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                /* isGroupDrag= */ true,
                                /* isMultiTabDrag= */ false)));
        assertFalse(
                "Multi-tab drag started should return false.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                /* isGroupDrag= */ false,
                                /* isMultiTabDrag= */ true)));
    }

    @Test
    public void testOnDrag_ActionDrop_TabSwitcher() {
        doTestOnDragActionDropInTabSwitcher(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_TabSwitcher_TabGroup() {
        doTestOnDragActionDropInTabSwitcher(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_TabSwitcher_MultiTab() {
        doTestOnDragActionDropInTabSwitcher(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);
    }

    private void doTestOnDragActionDropInTabSwitcher(boolean isGroupDrag, boolean isMultiTabDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result",
                        getTabSelectionType(isGroupDrag, isMultiTabDrag));
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_TAB_SWITCHER)
                        .expectNoRecords(resultHistogram + ".DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .build();
        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));

        // Drop should return false, since it is trying to drop into tab switcher.
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DROP,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    @Test
    public void testOnDrag_ActionDrop_SameInstance() {
        doTestOnDragActionDropInSameInstance(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_SameInstance_TabGroup() {
        doTestOnDragActionDropInSameInstance(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_SameInstance_MultiTab() {
        doTestOnDragActionDropInSameInstance(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);
    }

    private void doTestOnDragActionDropInSameInstance(boolean isGroupDrag, boolean isMultiTabDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result",
                        getTabSelectionType(isGroupDrag, isMultiTabDrag));
        AppHeaderUtils.setAppInDesktopWindowForTesting(true);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_SAME_INSTANCE)
                        .expectIntRecord(
                                resultHistogram + ".DesktopWindow",
                                DragDropResult.IGNORED_SAME_INSTANCE)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .build();
        setGlobalStateData(isGroupDrag, isMultiTabDrag);
        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
        // Drop should return false, since the destination instance is the same as the source
        // instance.
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DROP,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    @Test
    public void testOnDrag_ActionDrop_Success() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ false,
                /* isMultiTabDrag= */ false);

        // Verify user action `TabRemovedFromGroup` is not recorded.
        assertEquals(
                "TabRemovedFromGroup should not be recorded as the tab being dragged is not in a"
                        + " tab group",
                0,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    public void testOnDrag_ActionDrop_Success_DesktopWindow() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ false,
                /* isMultiTabDrag= */ false);

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
                        new ChromeTabDropDataAndroid.Builder()
                                .withTab(mTab)
                                .withTabInGroup(true)
                                .build());

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ false,
                /* isMultiTabDrag= */ false);

        // Verify user action `TabRemovedFromGroup` is recorded.
        assertEquals(
                "TabRemovedFromGroup should be recorded",
                1,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    public void testOnDrag_ActionDrop_Success_TabGroup_DesktopWindow() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ true,
                /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_Success_TabGroup() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ true,
                /* isMultiTabDrag= */ false);
    }

    @Test
    public void testOnDrag_ActionDrop_Success_MultiTab() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ false,
                /* isMultiTabDrag= */ true);
    }

    @Test
    public void testOnDrag_ActionDrop_Success_MultiTab_DesktopWindow() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);

        // Verify action drop is success.
        verifyActionDropSuccess(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ false,
                /* isMultiTabDrag= */ true);
    }

    private void verifyActionDropSuccess(
            boolean isInDesktopWindow, boolean isGroupDrag, boolean isMultiTabDrag) {
        String histogram =
                String.format(
                        "Android.DragDrop.%s.Type",
                        getTabSelectionType(isGroupDrag, isMultiTabDrag));
        AppHeaderUtils.setAppInDesktopWindowForTesting(isInDesktopWindow);

        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogram, DragDropType.TAB_STRIP_TO_CONTENT)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result")
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result")
                        .expectNoRecords(
                                "Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow");
        if (isInDesktopWindow) {
            builder.expectIntRecord(
                    histogram + ".DesktopWindow", DragDropType.TAB_STRIP_TO_CONTENT);
        } else {
            builder.expectNoRecords(histogram + ".DesktopWindow");
        }
        HistogramWatcher histogramWatcher = builder.build();

        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));

        // Drop should return true, since the destination instance is not the same as the source
        // instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        assertTrue(
                "Action drop should return true",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DROP,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDrag_ActionDragEnded_ReenableSearchBox() {
        // Setup drag drop global state.
        setGlobalStateData(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);

        // Mock current tab is a NTP.
        when(mCurrentTab.getNativePage()).thenReturn(mOriginalNtp);
        when(mTab.getNativePage()).thenReturn(mCurrentNtp);

        // Trigger drag start to capture the disabled NTP.
        mChromeTabbedOnDragListener.onDrag(
                mCompositorViewHolder,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_STARTED,
                        /* result= */ false,
                        /* isGroupDrag= */ false,
                        /* isMultiTabDrag= */ false));

        // Change the selected tab to a different NTP.
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        // Trigger drag end.
        mChromeTabbedOnDragListener.onDrag(
                mCompositorViewHolder,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_ENDED,
                        /* result= */ false,
                        /* isGroupDrag= */ false,
                        /* isMultiTabDrag= */ false));

        // Verify NTP search boxes are re-enabled.
        verify(mOriginalNtp).enableSearchBoxEditText(true);
        verify(mCurrentNtp).enableSearchBoxEditText(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Success() {
        verifyDropToDifferentModelSuccess(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Success_TabGroup() {
        verifyDropToDifferentModelSuccess(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Success_MultiTab() {
        verifyDropToDifferentModelSuccess(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Fail_IncognitoAsNewWindow() {
        verifyDropToDifferentModelFailed(/* isGroupDrag= */ false, /* isMultiTabDrag= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Fail_TabGroup_IncognitoAsNewWindow() {
        verifyDropToDifferentModelFailed(/* isGroupDrag= */ true, /* isMultiTabDrag= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnDrag_ActionDrop_DifferentModel_Fail_MultiTab_IncognitoAsNewWindow() {
        verifyDropToDifferentModelFailed(/* isGroupDrag= */ false, /* isMultiTabDrag= */ true);
    }

    private void verifyDropToDifferentModelSuccess(boolean isGroupDrag, boolean isMultiTabDrag) {
        // Destination tab model is incognito.
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mCurrentTab.isIncognitoBranded()).thenReturn(true);

        // Setup drag drop global state.
        setGlobalStateData(isGroupDrag, isMultiTabDrag);

        // Verify action drop is success.
        verifyActionDropSuccess(/* isInDesktopWindow= */ false, isGroupDrag, isMultiTabDrag);
    }

    private void verifyDropToDifferentModelFailed(boolean isGroupDrag, boolean isMultiTabDrag) {
        // Setup drag drop global state.
        setGlobalStateData(isGroupDrag, isMultiTabDrag);

        // Destination tab model is incognito.
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mCurrentTab.isIncognitoBranded()).thenReturn(true);

        // Call drag start to set states.
        assertTrue(
                "Drag started should return true.",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));

        // Drop should return false.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);

        // Verify action drop is failed.
        assertFalse(
                "Action drop should return false",
                mChromeTabbedOnDragListener.onDrag(
                        mCompositorViewHolder,
                        mockDragEvent(
                                DragEvent.ACTION_DROP,
                                /* result= */ false,
                                isGroupDrag,
                                isMultiTabDrag)));
    }

    private DragEvent mockDragEvent(
            int action, boolean result, boolean isGroupDrag, boolean isMultiTabDrag) {
        ClipDescription clipDescription;
        if (isGroupDrag) {
            clipDescription = mTabGroupClipDescription;
        } else if (isMultiTabDrag) {
            clipDescription = mMultiTabClipDescription;
        } else {
            clipDescription = mTabClipDescription;
        }
        return mockDragEvent(action, result, clipDescription);
    }

    private DragEvent mockDragEvent(int action, boolean result, ClipDescription clipDescription) {
        DragEvent event = Mockito.mock(DragEvent.class);
        when(event.getResult()).thenReturn(result);
        when(event.getClipDescription()).thenReturn(clipDescription);
        doReturn(action).when(event).getAction();
        return event;
    }

    private void setGlobalStateData(boolean isGroupDrag, boolean isMultiTabDrag) {
        if (isGroupDrag) {
            when(mDragDropGlobalState.getData())
                    .thenReturn(
                            new ChromeTabGroupDropDataAndroid.Builder()
                                    .withTabGroupMetadata(mTabGroupMetadata)
                                    .build());
        } else if (isMultiTabDrag) {
            when(mDragDropGlobalState.getData())
                    .thenReturn(
                            new ChromeMultiTabDropDataAndroid.Builder()
                                    .withTabs(Arrays.asList(mTab))
                                    .build());
        } else {
            when(mDragDropGlobalState.getData())
                    .thenReturn(new ChromeTabDropDataAndroid.Builder().withTab(mTab).build());
        }
    }

    private String getTabSelectionType(boolean isGroupDrag, boolean isMultiTabDrag) {
        if (isGroupDrag) {
            return "TabGroup";
        } else if (isMultiTabDrag) {
            return "MultiTab";
        } else {
            return "Tab";
        }
    }
}
