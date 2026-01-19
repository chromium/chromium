// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link EducationalTipModuleTwoCellBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleTwoCellBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mBuildCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private SetupListManager mSetupListManager;

    private Context mContext;
    private EducationalTipModuleTwoCellBuilder mModuleBuilder;

    @Before
    public void setUp() {
        SetupListManager.setInstanceForTesting(mSetupListManager);
        when(mSetupListManager.isSetupListActive()).thenReturn(true);

        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @SmallTest
    public void testBuild() {
        mModuleBuilder = new EducationalTipModuleTwoCellBuilder(mActionDelegate);
        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback).onResult(any(EducationalTipModuleTwoCellCoordinator.class));
    }

    @Test
    @SmallTest
    public void testCreateView() {
        mModuleBuilder = new EducationalTipModuleTwoCellBuilder(mActionDelegate);
        ViewGroup parentView = new FrameLayout(mContext);
        ViewGroup moduleView = mModuleBuilder.createView(parentView);
        assertNotNull(moduleView);
        assertTrue(moduleView instanceof EducationalTipModuleTwoCellView);
    }

    @Test
    @SmallTest
    public void testGetManualRank_SetupListActive() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.shouldShowTwoCellLayout()).thenReturn(true);
        mModuleBuilder = new EducationalTipModuleTwoCellBuilder(mActionDelegate);
        Integer manualOrder = mModuleBuilder.getManualRank();
        assertNotNull("Manual order should be present when setup list is active", manualOrder);
        assertEquals(0, manualOrder.intValue());
    }

    @Test
    @SmallTest
    public void testGetManualRank_SetupListInActive() {
        when(mSetupListManager.isSetupListActive()).thenReturn(false);
        when(mSetupListManager.shouldShowTwoCellLayout()).thenReturn(true);
        mModuleBuilder = new EducationalTipModuleTwoCellBuilder(mActionDelegate);
        Integer manualOrder = mModuleBuilder.getManualRank();
        assertNull("Manual order should be null when setup list is inactive", manualOrder);
    }

    @Test
    @SmallTest
    public void testCreateInputContext() {
        mModuleBuilder = new EducationalTipModuleTwoCellBuilder(mActionDelegate);
        InputContext inputContext = mModuleBuilder.createInputContext();
        assertNull(inputContext);
    }
}
