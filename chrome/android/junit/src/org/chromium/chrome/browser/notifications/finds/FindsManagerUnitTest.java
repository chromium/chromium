// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.finds.FindsService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link FindsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FindsManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private FindsService mFindsService;
    @Mock private PrefService mPrefService;

    private FindsManager mManager;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        UserPrefs.setPrefServiceForTesting(mPrefService);

        mManager =
                new FindsManager(
                        mActivity,
                        mProfile,
                        mBottomSheetController,
                        mSnackbarManager,
                        mFindsService);
    }

    @Test
    public void testConstructionRegistersObserver() {
        verify(mFindsService).addObserver(mManager);
    }

    @Test
    public void testConstructionReschedulesNotifications() {
        verify(mFindsService).maybeRescheduleNotifications();
    }

    @Test
    public void testDestroyUnregistersObserver() {
        mManager.destroy();
        verify(mFindsService).removeObserver(mManager);
    }

    @Test
    public void testOnOptInCriteriaFulfilledShowsBottomSheet() {
        mManager.onOptInCriteriaFulfilled();
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    @EnableFeatures("ChromeFinds:always_show_opt_in_promo/true")
    public void testAlwaysShowOptInPromoTriggersOnConstruction() {
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }
}
