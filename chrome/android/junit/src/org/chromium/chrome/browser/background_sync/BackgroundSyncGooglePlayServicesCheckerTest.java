// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.google.android.gms.common.ConnectionResult;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.gms.shadows.ShadowChromiumPlayServicesAvailability;

/** Unit tests for GooglePlayServicesChecker. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowChromiumPlayServicesAvailability.class})
public class BackgroundSyncGooglePlayServicesCheckerTest {
    @Test
    @Feature("BackgroundSync")
    public void testDisableLogicWhenGooglePlayServicesReturnsSuccess() {
        ShadowChromiumPlayServicesAvailability.setGetGooglePlayServicesConnectionResult(
                ConnectionResult.SUCCESS);
        assertFalse(GooglePlayServicesChecker.shouldDisableBackgroundSync());
    }

    @Test
    @Feature("BackgroundSync")
    public void testDisableLogicWhenGooglePlayServicesReturnsError() {
        ShadowChromiumPlayServicesAvailability.setGetGooglePlayServicesConnectionResult(
                ConnectionResult.SERVICE_VERSION_UPDATE_REQUIRED);
        assertTrue(GooglePlayServicesChecker.shouldDisableBackgroundSync());
    }
}
