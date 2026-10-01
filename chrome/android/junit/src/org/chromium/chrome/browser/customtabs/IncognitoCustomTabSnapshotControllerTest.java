// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.os.Build;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Robolectric tests for {@link IncognitoCustomTabSnapshotController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoCustomTabSnapshotControllerTest {
    /** Activity that records setRecentsScreenshotEnabled() calls. */
    private static class FakeActivity extends Activity {
        private final List<Boolean> mRecentsScreenshotEnabledCalls = new ArrayList<>();

        @Override
        public void setRecentsScreenshotEnabled(boolean enabled) {
            mRecentsScreenshotEnabledCalls.add(enabled);
        }
    }

    private FakeActivity mActivity;
    private boolean mIsIncognitoShowing;
    private final Supplier<Boolean> mIsIncognitoShowingSupplier = () -> mIsIncognitoShowing;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FakeActivity.class).get();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsAdded() {
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivity, mIsIncognitoShowingSupplier);

        assertEquals(
                WindowManager.LayoutParams.FLAG_SECURE,
                mActivity.getWindow().getAttributes().flags
                        & WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertTrue(mActivity.mRecentsScreenshotEnabledCalls.isEmpty());
        }
    }

    @Test
    @EnableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsRemoved() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        mIsIncognitoShowing = true;
        new IncognitoCustomTabSnapshotController(mActivity, mIsIncognitoShowingSupplier);

        assertEquals(
                0,
                mActivity.getWindow().getAttributes().flags
                        & WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(false), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }
}
