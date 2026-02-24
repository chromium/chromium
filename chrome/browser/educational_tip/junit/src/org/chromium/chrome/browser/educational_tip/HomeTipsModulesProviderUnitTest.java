// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellBuilder;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

import java.util.Collection;
import java.util.List;

/** Test relating to {@link HomeTipsModulesProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeTipsModulesProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private SetupListManager mSetupListManager;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        when(mActionDelegate.getProfileSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(mProfile));
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        SetupListManager.setInstanceForTesting(mSetupListManager);
        when(mSetupListManager.getRankedModuleTypes())
                .thenReturn(
                        List.of(
                                ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                                ModuleType.ADDRESS_BAR_PLACEMENT_PROMO));
        when(mSetupListManager.getTwoCellContainerModuleTypes())
                .thenReturn(List.of(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER));
    }

    @Test
    @SmallTest
    public void testRegisterTipModules_TwoCell() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.shouldShowTwoCellLayout()).thenReturn(true);

        HomeTipsModulesProvider.registerTipModules(mActionDelegate, mModuleRegistry);

        verify(mModuleRegistry)
                .registerModule(
                        eq(SetupListModuleUtils.getTwoCellContainerModuleTypes().get(0)),
                        any(EducationalTipModuleTwoCellBuilder.class));
    }

    @Test
    @SmallTest
    public void testRegisterTipModules_SetupListSingleCell() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.shouldShowTwoCellLayout()).thenReturn(false);

        HomeTipsModulesProvider.registerTipModules(mActionDelegate, mModuleRegistry);

        List<Integer> setupListModules = SetupListManager.BASE_SETUP_LIST_ORDER;
        for (@ModuleType int moduleType : setupListModules) {
            verify(mModuleRegistry)
                    .registerModule(eq(moduleType), any(EducationalTipModuleBuilder.class));
        }
    }

    @Test
    @SmallTest
    public void testRegisterTipModules_EducationalTips() {
        when(mSetupListManager.isSetupListActive()).thenReturn(false);

        HomeTipsModulesProvider.registerTipModules(mActionDelegate, mModuleRegistry);

        Collection<Integer> educationalTipModules = EducationalTipModuleUtils.getModuleTypes();
        for (@ModuleType int moduleType : educationalTipModules) {
            verify(mModuleRegistry)
                    .registerModule(eq(moduleType), any(EducationalTipModuleBuilder.class));
        }
    }

    @Test
    @SmallTest
    public void testGetModulesToRegister_returnsEducationalTipsWhenInactive() {
        Collection<Integer> expectedModules = EducationalTipModuleUtils.getModuleTypes();
        Collection<Integer> actualModules =
                HomeTipsModulesProvider.getModuleTypesToRegister(
                        /* isSetupListActive= */ false, /* showTwoCell= */ false);
        assertArrayEquals(expectedModules.toArray(), actualModules.toArray());
    }

    @Test
    @SmallTest
    public void
            testGetModulesToRegister_returnsTwoCellContainerWhenSetupListActiveAndTwoCellEnabled() {
        Collection<Integer> actualModules =
                HomeTipsModulesProvider.getModuleTypesToRegister(
                        /* isSetupListActive= */ true, /* showTwoCell= */ true);
        assertTrue(actualModules.contains(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER));
    }
}
