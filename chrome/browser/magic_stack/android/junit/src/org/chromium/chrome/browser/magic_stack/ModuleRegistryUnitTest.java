// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.HOME_MODULE_PREF_REFACTOR;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getEducationalTipModuleList;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link ModuleRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModuleRegistryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int REGISTERED_MODULE_TYPE = 0;
    private static final int UNREGISTERED_MODULE_TYPE = 1;

    @Mock private ModuleProviderBuilder mModuleProviderBuilder;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mOnModuleBuiltCallback;
    @Mock private SimpleRecyclerViewAdapter mAdapter;
    @Mock private ModuleRegistry.OnViewCreatedCallback mOnViewCreatedCallback;
    @Mock private ActivityLifecycleDispatcherImpl mActivityLifecycleDispatcher;
    @Captor private ArgumentCaptor<PauseResumeWithNativeObserver> mLifecycleObserverArgumentCaptor;

    private ModuleRegistry mModuleRegistry;
    private HomeModulesConfigManager mHomeModulesConfigManager;

    @Before
    public void setUp() {
        mHomeModulesConfigManager = new HomeModulesConfigManager();
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        mModuleRegistry =
                new ModuleRegistry(mHomeModulesConfigManager, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher).register(mLifecycleObserverArgumentCaptor.capture());
    }

    @After
    public void tearDown() {
        mModuleRegistry.destroy();
    }

    @Test
    @SmallTest
    public void testBuild() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);

        mModuleRegistry.build(UNREGISTERED_MODULE_TYPE, mModuleDelegate, mOnModuleBuiltCallback);
        verify(mModuleProviderBuilder, never())
                .build(eq(mModuleDelegate), eq(mOnModuleBuiltCallback));

        mModuleRegistry.build(REGISTERED_MODULE_TYPE, mModuleDelegate, mOnModuleBuiltCallback);
        verify(mModuleProviderBuilder).build(eq(mModuleDelegate), eq(mOnModuleBuiltCallback));
    }

    @Test
    @SmallTest
    public void testRegisterAdapter() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);

        // Verifies that only registered ModuleProviderBuilder will be added to the adapter.
        mModuleRegistry.registerAdapter(mAdapter, mOnViewCreatedCallback);
        verify(mAdapter).registerType(eq(REGISTERED_MODULE_TYPE), any(), any());
        verify(mAdapter, never()).registerType(eq(UNREGISTERED_MODULE_TYPE), any(), any());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);
        mModuleRegistry.destroy();
        verify(mModuleProviderBuilder).destroy();
        verify(mActivityLifecycleDispatcher).unregister(mLifecycleObserverArgumentCaptor.capture());
    }

    @Test
    @SmallTest
    public void testOnPauseWithNative() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);

        mLifecycleObserverArgumentCaptor.getValue().onPauseWithNative();
        verify(mModuleProviderBuilder).onPauseWithNative();
    }

    @Test
    @Features.DisableFeatures(HOME_MODULE_PREF_REFACTOR)
    public void testGetEnabledModuleList() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);

        // Verifies that a module is enabled if it is eligible to build and is enabled in settings.
        when(mModuleProviderBuilder.isEligible()).thenReturn(false);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(REGISTERED_MODULE_TYPE, true);
        assertTrue(mModuleRegistry.getEnabledModuleSet().isEmpty());

        when(mModuleProviderBuilder.isEligible()).thenReturn(true);
        Set<Integer> expectedSet = Set.of(REGISTERED_MODULE_TYPE);
        assertEquals(expectedSet, mModuleRegistry.getEnabledModuleSet());

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(REGISTERED_MODULE_TYPE, false);
        assertFalse(mHomeModulesConfigManager.getPrefModuleTypeEnabled(REGISTERED_MODULE_TYPE));
        assertTrue(mModuleRegistry.getEnabledModuleSet().isEmpty());
    }

    @Test
    @Features.EnableFeatures(HOME_MODULE_PREF_REFACTOR)
    public void testGetEnabledModuleSet_allCardsOff_restoresOnAndOffTypes() {
        registerModuleWithEligibility(ModuleType.SINGLE_TAB, true);
        registerModuleWithEligibility(ModuleType.PRICE_CHANGE, false);

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.SINGLE_TAB, true);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);

        Set<Integer> enabledModulesBeforeToggleOff = Set.of(ModuleType.SINGLE_TAB);
        assertEquals(enabledModulesBeforeToggleOff, mModuleRegistry.getEnabledModuleSet());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, false);

        Set<Integer> enabledModulesAfterToggleOff = mModuleRegistry.getEnabledModuleSet();
        assertTrue(enabledModulesAfterToggleOff.isEmpty());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, true);
        Set<Integer> enabledModulesAfterToggleOn = mModuleRegistry.getEnabledModuleSet();

        assertEquals(enabledModulesBeforeToggleOff, enabledModulesAfterToggleOn);
    }

    @Test
    @Features.EnableFeatures(HOME_MODULE_PREF_REFACTOR)
    public void testGetModuleListShownInSettings() {
        testGetModuleListShownInSettingsImpl();
    }

    @Test
    public void testGetModuleListShownInSettings_featureDisabled() {
        testGetModuleListShownInSettingsImpl();
    }

    private void testGetModuleListShownInSettingsImpl() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleProviderBuilder.isEligible()).thenReturn(false);
        assertTrue(mModuleRegistry.getModuleListShownInSettings().isEmpty());

        // Verifies the list contains the module which eligible to build.
        when(mModuleProviderBuilder.isEligible()).thenReturn(true);

        List<Integer> expectedList = List.of(REGISTERED_MODULE_TYPE);
        assertEquals(expectedList, mModuleRegistry.getModuleListShownInSettings());
    }

    @Test
    public void testGetEducationalTipListItemShown() {
        for (@ModuleDelegate.ModuleType int tipModule : getEducationalTipModuleList()) {
            registerModuleWithEligibility(tipModule, true);
        }

        // Verifies that the return list of getModuleListShownInSettings() contains only one of
        // educational tip modules.
        List<Integer> moduleList = mModuleRegistry.getModuleListShownInSettings();
        assertEquals(1, moduleList.size());
        assertTrue(HomeModulesUtils.belongsToEducationalTipModule(moduleList.get(0)));
    }

    @Test
    public void testGetMultipleListItemShown() {
        List<Integer> moduleTypeList =
                Arrays.asList(
                        SINGLE_TAB,
                        ModuleDelegate.ModuleType.SAFETY_HUB,
                        ModuleDelegate.ModuleType.QUICK_DELETE_PROMO,
                        ModuleDelegate.ModuleType.PRICE_CHANGE);

        for (Integer moduleType : moduleTypeList) {
            registerModuleWithEligibility(moduleType, true);
        }

        List<Integer> moduleList = mModuleRegistry.getModuleListShownInSettings();

        for (Integer moduleType : moduleTypeList) {
            assertTrue(moduleList.contains(moduleType));
        }
    }

    @Test
    public void testGetMultipleListItemShownSomeComplex() {
        List<Integer> eligibleTypeList =
                Arrays.asList(
                        ModuleDelegate.ModuleType.SAFETY_HUB,
                        ModuleDelegate.ModuleType.AUXILIARY_SEARCH);
        List<Integer> notEligibleTypeList =
                Arrays.asList(SINGLE_TAB, ModuleDelegate.ModuleType.PRICE_CHANGE);

        for (Integer moduleType : eligibleTypeList) {
            registerModuleWithEligibility(moduleType, true);
        }

        for (Integer moduleType : notEligibleTypeList) {
            registerModuleWithEligibility(moduleType, false);
        }

        List<Integer> moduleList = mModuleRegistry.getModuleListShownInSettings();

        // Verifies the return list of getModuleListShownInSettings() only contains eligible module
        // types.
        for (Integer moduleType : eligibleTypeList) {
            assertTrue(moduleList.contains(moduleType));
        }
    }

    private void registerModuleWithEligibility(@ModuleType int moduleType, boolean eligibility) {
        ModuleProviderBuilder builder = mock(ModuleProviderBuilder.class);
        when(builder.isEligible()).thenReturn(eligibility);
        mModuleRegistry.registerModule(moduleType, builder);
    }
}
