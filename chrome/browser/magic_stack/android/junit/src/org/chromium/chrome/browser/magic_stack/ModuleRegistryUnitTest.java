// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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

    private ModuleRegistry mModuleRegistry;

    @Before
    public void setUp() {
        mModuleRegistry = ModuleRegistry.getInstance();
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
    public void testHasModuleCanBeCustomized() {
        mModuleRegistry.registerModule(ModuleType.SINGLE_TAB, mModuleProviderBuilder1);
        when(mModuleProviderBuilder1.isEligible()).thenReturn(true);
        // Verifies that the ModuleType.SINGLE_TAB can't be customized.
        assertFalse(mModuleRegistry.hasCustomizableModule());

        mModuleRegistry.registerModule(ModuleType.PRICE_CHANGE, mModuleProviderBuilder2);
        when(mModuleProviderBuilder2.isEligible()).thenReturn(false);
        assertFalse(mModuleRegistry.hasCustomizableModule());

        when(mModuleProviderBuilder2.isEligible()).thenReturn(true);
        assertTrue(mModuleRegistry.hasCustomizableModule());
    }
}
