// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity;

import static androidx.browser.trusted.TrustedWebActivityIntentBuilder.EXTRA_SPLASH_SCREEN_PARAMS;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.browser.trusted.splashscreens.SplashScreenParamKey;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;

import java.util.function.Supplier;

/** Tests for {@link TwaSplashController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TwaSplashControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public Activity mActivity;
    @Mock public SplashController mSplashController;
    @Mock public Supplier<SplashController> mSplashControllerSupplier;
    @Mock public BrowserServicesIntentDataProvider mIntentDataProvider;

    private Intent mIntent;
    private Bundle mSplashParams;

    @Captor public ArgumentCaptor<Long> mDurationCaptor;

    @Before
    public void setUp() {
        doReturn(mSplashController).when(mSplashControllerSupplier).get();

        mIntent = new Intent();
        mSplashParams = new Bundle();
        mIntent.putExtra(EXTRA_SPLASH_SCREEN_PARAMS, mSplashParams);
        doReturn(mIntent).when(mIntentDataProvider).getIntent();
    }

    private void createController() {
        new TwaSplashController(
                mActivity,
                mSplashControllerSupplier,
                mIntentDataProvider);
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testDefaultDuration() {
        createController();
        verify(mSplashController).setConfigAndShowSplash(any(), mDurationCaptor.capture());
        assertEquals(0L, mDurationCaptor.getValue().longValue());
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testDurationWithinRange() {
        mSplashParams.putInt(SplashScreenParamKey.KEY_FADE_OUT_DURATION_MS, 500);
        createController();
        verify(mSplashController).setConfigAndShowSplash(any(), mDurationCaptor.capture());
        assertEquals(500L, mDurationCaptor.getValue().longValue());
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testDurationTooLarge() {
        mSplashParams.putInt(SplashScreenParamKey.KEY_FADE_OUT_DURATION_MS, 5000);
        createController();
        verify(mSplashController).setConfigAndShowSplash(any(), mDurationCaptor.capture());
        assertEquals(1000L, mDurationCaptor.getValue().longValue());
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testDurationNegative() {
        mSplashParams.putInt(SplashScreenParamKey.KEY_FADE_OUT_DURATION_MS, -500);
        createController();
        verify(mSplashController).setConfigAndShowSplash(any(), mDurationCaptor.capture());
        assertEquals(0L, mDurationCaptor.getValue().longValue());
    }
}
