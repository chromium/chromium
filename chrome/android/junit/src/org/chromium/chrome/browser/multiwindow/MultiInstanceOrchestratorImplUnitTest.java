// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;

import java.util.List;

/** Unit tests for {@link MultiInstanceOrchestratorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class MultiInstanceOrchestratorImplUnitTest {
    private static final int SOURCE_WINDOW_ID = 1;
    private static final int DEST_WINDOW_ID = 2;
    private static final int NONEXISTENT_INSTANCE_ID = 4;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ChromeTabbedActivity mTabbedActivity1;
    @Mock private ChromeTabbedActivity mTabbedActivity2;
    @Mock private MultiInstanceManager mMultiInstanceManager1;
    @Mock private MultiInstanceManager mMultiInstanceManager2;
    @Mock private TabReparentingDelegate mTabReparentingDelegate;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private MultiInstanceOrchestrator mMultiInstanceOrchestrator;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiInstanceOrchestratorImpl.setTabReparentingDelegateForTesting(mTabReparentingDelegate);
        mMultiInstanceOrchestrator = MultiInstanceOrchestratorImpl.getInstance();
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity2, mMultiInstanceManager2);
        createTestInstance(SOURCE_WINDOW_ID);
        createTestInstance(DEST_WINDOW_ID);

        when(mTab1.getContext()).thenReturn(mTabbedActivity1);
        when(mTab2.getContext()).thenReturn(mTabbedActivity1);
        when(mTabbedActivity1.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        MultiWindowUtils.setActivitySupplierForTesting(() -> mTabbedActivity2);
    }

    @After
    public void teardown() {
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensAdjacently() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensFullScreen() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_inMultiWindowMode_opensAdjacently() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabsToNewWindow_atInstanceLimit_showsMessageInTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabbedActivity1.getResources()).thenReturn(mock(Resources.class));

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabsToNewWindow_atInstanceLimit_noMessageShownWithNonTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);
        List<Tab> tabs = List.of(mTab1);
        when(mTab1.getContext()).thenReturn(mActivity);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_InvalidParams() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // destWindowId should have persisted instance state.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                /* destWindowId= */ NONEXISTENT_INSTANCE_ID,
                                tabs,
                                /* destTabIndex= */ 2,
                                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX));

        // destTabIndex and destGroupTabId should not both be specified when moving tabs to an
        // existing window.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                DEST_WINDOW_ID,
                                tabs,
                                /* destTabIndex= */ 1,
                                /* destGroupTabId= */ 2));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabIndex() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        int destTabIndex = 0;

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mTabbedActivity2), eq(tabs), eq(destTabIndex), eq(-1));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabGroup() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getTabGroupId()).thenReturn(null);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                /* destGroupTabId= */ 3);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(eq(mTabbedActivity2), eq(tabs), eq(-1), eq(3));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_withDestroyedActivity() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        MultiWindowUtils.setActivitySupplierForTesting(() -> null);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                /* destTabIndex= */ 0,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        eq(tabs),
                        eq(DEST_WINDOW_ID),
                        eq(true),
                        eq(null),
                        eq(NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY));
    }

    private void createTestInstance(int windowId) {
        MultiWindowTestUtils.createInstance(
                windowId, /* url= */ "https://www.google.com", /* tabCount= */ 1, windowId);
    }
}
