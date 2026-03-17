// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomBarBottomToolbarDelegateImplUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private HubColorMixer mHubColorMixer;

    private Activity mActivity;
    private ViewGroup mContainer;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContainer = new FrameLayout(mActivity);
    }

    @Test
    public void testIsBottomToolbarEnabled() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl(mActivity);
        assertTrue(delegate.isBottomToolbarEnabled());
        delegate.destroy();
    }

    @Test
    public void testGetBottomToolbarVisibilitySupplier() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl(mActivity);
        assertTrue(delegate.getBottomToolbarVisibilitySupplier().get());
        delegate.destroy();
    }

    @Test
    public void testInitializeBottomToolbarView() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl(mActivity);
        HubBottomToolbarView view =
                delegate.initializeBottomToolbarView(
                        mActivity, mContainer, mPaneManager, mHubColorMixer);

        assertNotNull(view);
        assertEquals(1, mContainer.getChildCount());
        assertEquals(view, mContainer.getChildAt(0));

        delegate.destroy();
    }

    @Test
    public void testAttachBottomBarView() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl(mActivity);
        HubBottomToolbarView parentView =
                delegate.initializeBottomToolbarView(
                        mActivity, mContainer, mPaneManager, mHubColorMixer);

        View childView = new View(mActivity);
        delegate.attachBottomBarView(childView);

        assertEquals(1, parentView.getChildCount());
        assertEquals(childView, parentView.getChildAt(0));

        delegate.destroy();
    }
}
