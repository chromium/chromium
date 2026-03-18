// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
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

    @Mock private Activity mCurrentActivity;
    @Mock private ChromeTabbedActivity mDestActivity;
    @Mock private TabReparentingDelegate mTabReparentingDelegate;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private MultiInstanceOrchestrator mMultiInstanceOrchestrator;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiInstanceOrchestratorImpl.setTabReparentingDelegateForTesting(mTabReparentingDelegate);
        mMultiInstanceOrchestrator = MultiInstanceOrchestratorImpl.getInstance();
        createTestInstance(SOURCE_WINDOW_ID);
        createTestInstance(DEST_WINDOW_ID);

        when(mTab1.getContext()).thenReturn(mCurrentActivity);
        when(mTab2.getContext()).thenReturn(mCurrentActivity);
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        MultiWindowUtils.setActivitySupplierForTesting(() -> mDestActivity);
    }

    @After
    public void teardown() {
        ApplicationStatus.destroyForJUnitTests();
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
        // Act.
        List<Tab> tabs = List.of(mTab1, mTab2);
        int destTabIndex = 0;
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mDestActivity), eq(tabs), eq(destTabIndex), eq(-1));
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
                .reparentTabsToExistingWindow(eq(mDestActivity), eq(tabs), eq(-1), eq(3));
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
