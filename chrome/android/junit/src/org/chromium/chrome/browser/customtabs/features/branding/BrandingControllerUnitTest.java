// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests AMP url handling in the CustomTab Toolbar.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
public class BrandingControllerUnitTest {
    @Rule
    public MockitoRule mTestRule = MockitoJUnit.rule();

    @Mock
    ToolbarBrandingDelegate mToolbarBrandingDelegate;
    @Mock
    ShadowPostTask.TestImpl mShadowPostTaskImpl;

    @Captor
    ArgumentCaptor<Runnable> mPostTaskRunnable;
    @Captor
    ArgumentCaptor<Long> mPostTaskDelay;

    private BrandingController mBrandingController;

    @Before
    public void setup() {
        doNothing()
                .when(mShadowPostTaskImpl)
                .postDelayedTask(any(), mPostTaskRunnable.capture(), mPostTaskDelay.capture());
        ShadowPostTask.setTestImpl(mShadowPostTaskImpl);

        mBrandingController = new BrandingController();
    }

    @After
    public void tearDown() {
        ShadowPostTask.reset();
        ShadowSystemClock.reset();
    }

    @Test
    public void testOnToolbarInitialized_Asap() {
        mBrandingController.onToolbarInitialized(mToolbarBrandingDelegate);

        verify(mToolbarBrandingDelegate).showEmptyLocationBar();
        verify(mToolbarBrandingDelegate).showBrandingLocationBar();
        verify(mToolbarBrandingDelegate, never()).showRegularToolbar();

        // Post task scheduled for show regular location bar.
        verify(mShadowPostTaskImpl)
                .postDelayedTask(any(), mPostTaskRunnable.capture(), mPostTaskDelay.capture());

        assertNotNull(mPostTaskRunnable.getValue());
        assertNotNull(mPostTaskDelay.getValue());

        mPostTaskRunnable.getValue().run();
        verify(mToolbarBrandingDelegate).showRegularToolbar();
        assertEquals("Show branding duration is different.",
                BrandingController.TOTAL_BRANDING_DELAY_MS, mPostTaskDelay.getValue().intValue());
    }
}
