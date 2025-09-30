// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link TipsPromoCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TipsPromoCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private TipsPromoCoordinator mTipsPromoCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTipsPromoCoordinator = new TipsPromoCoordinator(mActivity, mBottomSheetController);
    }

    @SmallTest
    @Test
    public void testDestroy() {
        BottomSheetContent bottomSheetContent =
                mTipsPromoCoordinator.getBottomSheetContentForTesting();
        bottomSheetContent.destroy();
    }

    @SmallTest
    @Test
    public void testShowBottomSheet() {
        mTipsPromoCoordinator.showBottomSheet(TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING);
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }
}
