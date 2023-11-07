// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link HubCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubCoordinatorUnitTest {
    private static final int TAB_ID = 7;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private HubLayoutController mHubLayoutController;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;

    private ObservableSupplierImpl<Tab> mTabSupplier = new ObservableSupplierImpl<>();
    private PaneManager mPaneManager;
    private FrameLayout mRootView;
    private HubCoordinator mHubCoordinator;

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mTab.getId()).thenReturn(TAB_ID);

        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane));
        mPaneManager = new PaneManagerImpl(builder);
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mRootView = new FrameLayout(activity);
        activity.setContentView(mRootView);

        mHubCoordinator =
                new HubCoordinator(mRootView, mPaneManager, mHubLayoutController, mTabSupplier);
        ShadowLooper.runUiThreadTasks();
        mRootView.getChildCount();
        assertNotEquals(0, mRootView.getChildCount());
    }

    @After
    public void tearDown() {
        mHubCoordinator.destroy();
        assertEquals(0, mRootView.getChildCount());
    }

    @Test
    @SmallTest
    public void testBackNavigationBetweenPanes() {
        assertFalse(mHubCoordinator.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertTrue(mHubCoordinator.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mHubCoordinator.handleBackPress());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertFalse(mHubCoordinator.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @SmallTest
    public void testBackNavigationWithNullTab() {
        assertFalse(mHubCoordinator.getHandleBackPressChangedSupplier().get());
        assertEquals(BackPressResult.FAILURE, mHubCoordinator.handleBackPress());

        mTabSupplier.set(mTab);
        assertTrue(mHubCoordinator.getHandleBackPressChangedSupplier().get());
        mTabSupplier.set(null);

        assertEquals(BackPressResult.FAILURE, mHubCoordinator.handleBackPress());
        verify(mHubLayoutController, never()).selectTabAndHideHubLayout(anyInt());
    }

    @Test
    @SmallTest
    public void testBackNavigationWithTab() {
        assertFalse(mHubCoordinator.getHandleBackPressChangedSupplier().get());
        assertEquals(BackPressResult.FAILURE, mHubCoordinator.handleBackPress());

        mTabSupplier.set(mTab);
        assertTrue(mHubCoordinator.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mHubCoordinator.handleBackPress());
        verify(mHubLayoutController).selectTabAndHideHubLayout(eq(TAB_ID));
    }

    @Test
    @SmallTest
    public void testBackNavigationPriority() {
        mTabSupplier.set(mTab);
        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertTrue(mHubCoordinator.getHandleBackPressChangedSupplier().get());

        // Between pane naviation
        assertEquals(BackPressResult.SUCCESS, mHubCoordinator.handleBackPress());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertTrue(mHubCoordinator.getHandleBackPressChangedSupplier().get());

        // Exit Hub navigation.
        assertEquals(BackPressResult.SUCCESS, mHubCoordinator.handleBackPress());
        verify(mHubLayoutController).selectTabAndHideHubLayout(eq(TAB_ID));
    }
}
