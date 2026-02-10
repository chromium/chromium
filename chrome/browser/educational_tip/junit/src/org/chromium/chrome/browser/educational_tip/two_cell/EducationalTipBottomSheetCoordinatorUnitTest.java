// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link EducationalTipBottomSheetCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipBottomSheetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final List<Integer> sRankedModuleTypes =
            new ArrayList<>(
                    List.of(
                            ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                            ModuleType.ADDRESS_BAR_PLACEMENT_PROMO,
                            ModuleType.SIGN_IN_PROMO,
                            ModuleType.SAVE_PASSWORDS_PROMO,
                            ModuleType.DEFAULT_BROWSER_PROMO));

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private EducationalTipBottomSheetCoordinator mEducationalTipBottomSheetCoordinator;

    @Before
    public void setUp() {
        when(mActionDelegate.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        SetupListModuleUtils.setRankedModuleTypesForTesting(sRankedModuleTypes);
        mEducationalTipBottomSheetCoordinator =
                new EducationalTipBottomSheetCoordinator(mActionDelegate);
        mEducationalTipBottomSheetCoordinator.showBottomSheet();

        verify(mBottomSheetController).requestShowContent(any(), /* animate= */ eq(true));
    }
}
