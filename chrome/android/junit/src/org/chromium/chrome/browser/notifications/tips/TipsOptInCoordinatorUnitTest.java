// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link TipsOptInCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsOptInCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private TipsOptInCoordinator mTipsOptInCoordinator;
    private SharedPreferencesManager mSharedPreferenceManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTipsOptInCoordinator = new TipsOptInCoordinator(mActivity, mBottomSheetController);
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet() {
        assertFalse(
                mSharedPreferenceManager.readBoolean(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, false));
        mTipsOptInCoordinator.showBottomSheet();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        assertTrue(
                mSharedPreferenceManager.readBoolean(TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, false));
    }
}
