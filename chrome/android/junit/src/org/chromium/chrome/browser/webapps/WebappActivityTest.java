// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link WebappActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 33)
public class WebappActivityTest {
    private static class TestWebappActivity extends WebappActivity {
        boolean callShouldDrawEdgeToEdgeOnCreate() {
            return shouldDrawEdgeToEdgeOnCreate();
        }

        boolean callCanColorStatusBarWithEdgeToEdgeHelper() {
            return canColorStatusBarWithEdgeToEdgeHelper();
        }

        boolean callCanSetTransparentStatusBarWithoutDelegate() {
            return canSetTransparentStatusBarWithoutDelegate();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void shouldDrawEdgeToEdgeOnCreateWithShortEdgesDisabled() {
        TestWebappActivity activity = new TestWebappActivity();

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void shouldNotDrawEdgeToEdgeOnCreateWithShortEdgesEnabled() {
        TestWebappActivity activity = new TestWebappActivity();

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void shortEdgesCanColorStatusBarWithoutDelegate() {
        TestWebappActivity activity = new TestWebappActivity();

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }
}
