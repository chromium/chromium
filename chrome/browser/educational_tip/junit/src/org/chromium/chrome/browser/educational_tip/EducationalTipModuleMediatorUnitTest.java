// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleMediator.FORCE_DEFAULT_BROWSER;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleMediator.FORCE_QUICK_DELETE;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleMediator.FORCE_TAB_GROUP;
import static org.chromium.chrome.browser.educational_tip.EducationalTipModuleMediator.FORCE_TAB_GROUP_SYNC;

import android.content.Context;

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

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Unit tests for {@link EducationalTipModuleMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mModel;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private FeatureList.TestValues mParamsTestValues;
    private Context mContext;
    private @ModuleType int mExpectedModuleType;
    private EducationalTipModuleMediator mEducationalTipModuleMediator;

    @Before
    public void setUp() {
        mParamsTestValues = new FeatureList.TestValues();
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        mExpectedModuleType = ModuleType.EDUCATIONAL_TIP;

        mEducationalTipModuleMediator =
                new EducationalTipModuleMediator(mModel, mModuleDelegate, mActionDelegate);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testShowModule() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // Test when no card can be displayed.
        mEducationalTipModuleMediator.showModule();
        verify(mModuleDelegate).onDataFetchFailed(mExpectedModuleType);
        verify(mModuleDelegate, never()).onDataReady(mExpectedModuleType, mModel);

        // Test showing default browser promo card.
        testShowModuleImpl(
                FORCE_DEFAULT_BROWSER,
                R.string.educational_tip_default_browser_title,
                R.string.educational_tip_default_browser_description,
                R.drawable.default_browser_promo_logo,
                /* timesOfCall= */ 1);

        // Test showing tab group promo card.
        testShowModuleImpl(
                FORCE_TAB_GROUP,
                R.string.educational_tip_tab_group_title,
                R.string.educational_tip_tab_group_description,
                R.drawable.tab_group_promo_logo,
                /* timesOfCall= */ 2);

        // Test showing tab group sync promo card.
        testShowModuleImpl(
                FORCE_TAB_GROUP_SYNC,
                R.string.educational_tip_tab_group_sync_title,
                R.string.educational_tip_tab_group_sync_description,
                R.drawable.tab_group_sync_promo_logo,
                /* timesOfCall= */ 3);

        // Test showing quick delete promo card.
        testShowModuleImpl(
                FORCE_QUICK_DELETE,
                R.string.educational_tip_quick_delete_title,
                R.string.educational_tip_quick_delete_description,
                R.drawable.quick_delete_promo_logo,
                /* timesOfCall= */ 4);
    }

    private void testShowModuleImpl(
            String enabledParam,
            int titleId,
            int descriptionId,
            int imageResource,
            int timesOfCall) {
        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, enabledParam, "true");
        FeatureList.setTestValues(mParamsTestValues);
        mEducationalTipModuleMediator.showModule();

        verify(mModel)
                .set(
                        EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING,
                        mContext.getString(titleId));
        verify(mModel)
                .set(
                        EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING,
                        mContext.getString(descriptionId));
        verify(mModel).set(EducationalTipModuleProperties.MODULE_CONTENT_IMAGE, imageResource);
        verify(mModuleDelegate, times(timesOfCall)).onDataReady(mExpectedModuleType, mModel);
        verify(mModuleDelegate).onDataFetchFailed(mExpectedModuleType);

        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, enabledParam, "false");
    }
}
