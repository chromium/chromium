// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link HubCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubCoordinatorUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;

    private PaneManager mPaneManager;
    private FrameLayout mRootView;

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);

        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, () -> mTabSwitcherPane)
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER, () -> mIncognitoTabSwitcherPane);
        mPaneManager = new PaneManagerImpl(builder);
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mRootView = new FrameLayout(activity);
        activity.setContentView(mRootView);
    }

    @Test
    @SmallTest
    public void testCreateAndDestroy() {
        HubCoordinator hubCoordinator = new HubCoordinator(mRootView, mPaneManager);
        mRootView.getChildCount();
        assertNotEquals(0, mRootView.getChildCount());
        hubCoordinator.destroy();
        assertEquals(0, mRootView.getChildCount());
    }

    @Test
    @SmallTest
    public void testBackNavigationBetweenPanes() {
        HubCoordinator hubCoordinator = new HubCoordinator(mRootView, mPaneManager);
        ShadowLooper.runUiThreadTasks();
        assertFalse(hubCoordinator.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertTrue(hubCoordinator.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, hubCoordinator.handleBackPress());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
        assertFalse(hubCoordinator.getHandleBackPressChangedSupplier().get());

        hubCoordinator.destroy();
    }
}
