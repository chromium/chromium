// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link PaneManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubManagerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Pane mTabSwitcherPane;

    private Activity mActivity;
    private FrameLayout mRootView;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mRootView = new FrameLayout(mActivity);
                            mActivity.setContentView(mRootView);
                        });
    }

    @Test
    @SmallTest
    public void testCreatesPaneManager() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, () -> mTabSwitcherPane);
        HubManager hubManager = HubManagerFactory.createHubManager(mActivity, builder);

        PaneManager paneManager = hubManager.getPaneManager();
        assertNotNull(paneManager);
        assertNull(paneManager.getFocusedPaneSupplier().get());
        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testHubController() {
        PaneListBuilder builder = new PaneListBuilder(new DefaultPaneOrderController());
        HubManagerImpl hubManager = new HubManagerImpl(mActivity, builder);
        HubController hubController = hubManager.getHubController();
        assertNull(hubManager.getHubCoordinatorForTesting());

        hubController.onHubLayoutShow();
        assertNotNull(hubManager.getHubCoordinatorForTesting());

        View containerView = hubController.getContainerView();
        assertNotNull(containerView);

        // Attach the container to the parent view.
        mRootView.addView(containerView);
        assertEquals(mRootView, containerView.getParent());

        hubController.onHubLayoutDoneHiding();
        assertNull(hubManager.getHubCoordinatorForTesting());

        // Container is still attached and will be removed separately.
        assertEquals(mRootView, containerView.getParent());
    }
}
