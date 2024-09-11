// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoBottomSheetContent;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.widget.ButtonCompat;

/** Test relating to {@link DefaultBrowserPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class DefaultBrowserPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private DefaultBrowserPromoCoordinator mDefaultBrowserPromoCoordinator;

    @Before
    public void setUp() {
        when(mActionDelegate.getContext()).thenReturn(RuntimeEnvironment.application);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mDefaultBrowserPromoCoordinator =
                new DefaultBrowserPromoCoordinator(mOnModuleClickedCallback, mActionDelegate);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testDefaultBrowserPromoCardBottomSheet() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        mDefaultBrowserPromoCoordinator.onCardClicked();

        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        DefaultBrowserPromoBottomSheetContent defaultBrowserBottomSheetContent =
                mDefaultBrowserPromoCoordinator.getDefaultBrowserBottomSheetContent();
        ButtonCompat bottomSheetButton =
                defaultBrowserBottomSheetContent
                        .getContentView()
                        .findViewById(
                                org.chromium.chrome.browser.educational_tip.R.id
                                        .default_browser_bottom_sheet_button);

        bottomSheetButton.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean(), anyInt());
        verify(mOnModuleClickedCallback).run();
    }
}
