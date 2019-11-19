// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.gms.Shadows;
import org.robolectric.shadows.gms.common.ShadowGoogleApiAvailability;

import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for GooglePlayServicesChecker. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGoogleApiAvailability.class})
public class BackgroundSyncGooglePlayServicesCheckerTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Test
    @Feature("BackgroundSync")
    public void testDisableLogicWhenGooglePlayServicesReturnsSuccess() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SUCCESS);
        assertFalse(GooglePlayServicesChecker.shouldDisableBackgroundSync());
    }

    @Test
    @Feature("BackgroundSync")
    public void testDisableLogicWhenGooglePlayServicesReturnsError() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SERVICE_VERSION_UPDATE_REQUIRED);
        assertTrue(GooglePlayServicesChecker.shouldDisableBackgroundSync());
    }
}
