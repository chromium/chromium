// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AutofillFallbackSurfaceLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE)
public class AutofillFallbackSurfaceLauncherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
    }

    @Test
    public void testOpenManagePasses_OpensNewActivity() {
        AutofillFallbackSurfaceLauncher.openGoogleWalletPassesPage(mWindowAndroid);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(cctIntent);
        assertEquals(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL, cctIntent.getDataString());
    }
}
