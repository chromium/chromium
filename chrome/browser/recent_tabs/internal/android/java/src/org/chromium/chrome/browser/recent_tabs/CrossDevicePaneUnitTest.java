// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationType;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneId;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link CrossDevicePane}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDevicePaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private HubContainerView mHubContainerView;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;

    private CrossDevicePane mCrossDevicePane;

    @Before
    public void setUp() {
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);

        mCrossDevicePane =
                new CrossDevicePaneImpl(
                        ApplicationProvider.getApplicationContext(), mOnToolbarAlphaChange);
    }

    @Test
    public void testPaneId() {
        assertEquals(PaneId.CROSS_DEVICE, mCrossDevicePane.getPaneId());
    }

    @Test
    public void testGetRootView() {
        assertNotNull(mCrossDevicePane.getRootView());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        mCrossDevicePane.notifyLoadHint(LoadHint.COLD);
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testNotifyLoadHint() {
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());

        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mCrossDevicePane.getRootView().getChildCount());

        mCrossDevicePane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testCreateFadeOutAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mCrossDevicePane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    public void testCreateFadeInAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mCrossDevicePane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }
}
