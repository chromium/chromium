// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link EducationalTipModuleBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mBuildCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private EducationalTipModuleBuilder mModuleBuilder;

    @Before
    public void setUp() {
        mModuleBuilder = new EducationalTipModuleBuilder(mActionDelegate);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testBuildEducationalTipModule_NotEligible() {
        assertFalse(ChromeFeatureList.sEducationalTipModule.isEnabled());

        assertFalse(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, never()).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testBuildEducationalTipModule_Eligible() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback).onResult(any(ModuleProvider.class));
    }
}
