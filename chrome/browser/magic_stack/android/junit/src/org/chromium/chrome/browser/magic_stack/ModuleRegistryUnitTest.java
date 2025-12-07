// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.INVALID_FRESHNESS_SCORE;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link ModuleRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModuleRegistryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int REGISTERED_MODULE_TYPE = 0;
    private static final int UNREGISTERED_MODULE_TYPE = 1;

    @Mock private ModuleProviderBuilder mModuleProviderBuilder1;
    @Mock private ModuleProviderBuilder mModuleProviderBuilder2;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mOnModuleBuiltCallback;
    @Mock private SimpleRecyclerViewAdapter mAdapter;
    @Mock private ModuleRegistry.OnViewCreatedCallback mOnViewCreatedCallback;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private ActivityLifecycleDispatcherImpl mActivityLifecycleDispatcher;
    @Captor private ArgumentCaptor<PauseResumeWithNativeObserver> mLifecycleObserverArgumentCaptor;

    private ModuleRegistry mModuleRegistry;

    @Before
    public void setUp() {
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
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder1);

        mModuleRegistry.build(UNREGISTERED_MODULE_TYPE, mModuleDelegate, mOnModuleBuiltCallback);
        verify(mModuleProviderBuilder1, never())
                .build(eq(mModuleDelegate), eq(mOnModuleBuiltCallback));

        mModuleRegistry.build(REGISTERED_MODULE_TYPE, mModuleDelegate, mOnModuleBuiltCallback);
        verify(mModuleProviderBuilder1).build(eq(mModuleDelegate), eq(mOnModuleBuiltCallback));
    }

    @Test
    @SmallTest
    public void testRegisterAdapter() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder1);

        // Verifies that only registered ModuleProviderBuilder will be added to the adapter.
        mModuleRegistry.registerAdapter(mAdapter, mOnViewCreatedCallback);
        verify(mAdapter).registerType(eq(REGISTERED_MODULE_TYPE), any(), any());
        verify(mAdapter, never()).registerType(eq(UNREGISTERED_MODULE_TYPE), any(), any());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder1);
        mModuleRegistry.destroy();
        verify(mModuleProviderBuilder1).destroy();
        verify(mActivityLifecycleDispatcher).unregister(mLifecycleObserverArgumentCaptor.capture());
    }

    @Test
    @SmallTest
    public void testOnPauseWithNative() {
        mModuleRegistry.registerModule(REGISTERED_MODULE_TYPE, mModuleProviderBuilder1);

        mLifecycleObserverArgumentCaptor.getValue().onPauseWithNative();
        verify(mModuleProviderBuilder1).onPauseWithNative();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateContextInput() {
        @ModuleType int moduleType1 = ModuleType.PRICE_CHANGE;
        @ModuleType int moduleType2 = ModuleType.DEFAULT_BROWSER_PROMO;
        Set<Integer> mEnabledModuleSet = new HashSet<>(Set.of(moduleType1, moduleType2));

        // Registers a solid module.
        InputContext inputs1 = HomeModulesUtils.createInputContext(moduleType1);
        when(mModuleProviderBuilder1.createInputContext()).thenReturn(inputs1);

        // Registers an ephemeral module.
        InputContext inputs2 = new InputContext();
        inputs2.addEntry("key", ProcessedValue.fromFloat(1.0f));
        when(mModuleProviderBuilder2.createInputContext()).thenReturn(inputs2);

        when(mHomeModulesConfigManager.getEnabledModuleSet()).thenReturn(mEnabledModuleSet);
        mModuleRegistry.registerModule(moduleType1, mModuleProviderBuilder1);
        mModuleRegistry.registerModule(moduleType2, mModuleProviderBuilder2);

        InputContext inputContext = mModuleRegistry.createInputContext();

        assertEquals(2, inputContext.getSizeForTesting());
        verify(mModuleProviderBuilder1).createInputContext();
        verify(mModuleProviderBuilder2).createInputContext();

        assertEquals(
                INVALID_FRESHNESS_SCORE,
                inputContext.getEntryValue(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType1))
                        .floatValue,
                0.01);
        assertEquals(1.0f, inputContext.getEntryValue("key").floatValue, 0.01);
    }
}
