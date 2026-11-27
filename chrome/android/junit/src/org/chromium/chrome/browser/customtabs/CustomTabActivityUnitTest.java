// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;

/** Unit tests for {@link CustomTabActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabActivityUnitTest {
    /** Records the value passed to setAllowCrossUidActivitySwitchFromBelow(). */
    public static class TestActivity extends Activity {
        public @Nullable Boolean mAllowCrossUidActivitySwitchFromBelow;

        @Override
        public void setAllowCrossUidActivitySwitchFromBelow(boolean allowed) {
            mAllowCrossUidActivitySwitchFromBelow = allowed;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;

    private TestActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testAllowCrossUidActivitySwitchFromBelow_targetNetwork() {
        when(mIntentDataProvider.hasTargetNetwork()).thenReturn(true);
        CustomTabActivity.maybeAllowCrossUidActivitySwitchFromBelow(mActivity, mIntentDataProvider);
        assertTrue(mActivity.mAllowCrossUidActivitySwitchFromBelow);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testAllowCrossUidActivitySwitchFromBelow_noTargetNetwork() {
        when(mIntentDataProvider.hasTargetNetwork()).thenReturn(false);
        CustomTabActivity.maybeAllowCrossUidActivitySwitchFromBelow(mActivity, mIntentDataProvider);
        assertNull(mActivity.mAllowCrossUidActivitySwitchFromBelow);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testAllowCrossUidActivitySwitchFromBelow_preVanillaIceCream() {
        when(mIntentDataProvider.hasTargetNetwork()).thenReturn(true);
        CustomTabActivity.maybeAllowCrossUidActivitySwitchFromBelow(mActivity, mIntentDataProvider);
        assertNull(mActivity.mAllowCrossUidActivitySwitchFromBelow);
    }
}
