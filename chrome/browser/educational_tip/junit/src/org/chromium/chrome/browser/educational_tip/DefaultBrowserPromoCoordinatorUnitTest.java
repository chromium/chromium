// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoBottomSheetContent;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoMetrics.DefaultBrowserPromoSourceType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.widget.ButtonCompat;

/** Test relating to {@link DefaultBrowserPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowAppCompatResources.class})
public class DefaultBrowserPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private DefaultBrowserPromoCoordinator mDefaultBrowserPromoCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mActionDelegate.getContext()).thenReturn(mActivity);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mDefaultBrowserPromoCoordinator =
                new DefaultBrowserPromoCoordinator(mOnModuleClickedCallback, mActionDelegate);
    }

    @Test
    public void testDefaultBrowserPromoCardBottomSheet() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DefaultBrowserPromo.Click",
                                DefaultBrowserPromoSourceType.EDUCATIONAL_TIP_PROMO)
                        .build();

        mDefaultBrowserPromoCoordinator.onCardClicked();

        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        DefaultBrowserPromoBottomSheetContent defaultBrowserBottomSheetContent =
                mDefaultBrowserPromoCoordinator.getDefaultBrowserBottomSheetContent();
        ButtonCompat bottomSheetButton =
                defaultBrowserBottomSheetContent.getContentView().findViewById(R.id.button);

        bottomSheetButton.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean(), anyInt());
        verify(mOnModuleClickedCallback).run();
        histogramWatcher.assertExpected();
    }
}
