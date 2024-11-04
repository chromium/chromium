// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Unit tests for {@link EducationalTipModuleMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PropertyModel mModel;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private ObservableSupplier<Profile> mProfileSupplier;
    @Mock private Profile mProfile;
    @Mock EducationalTipModuleMediator.Natives mEducationalTipModuleMediatorJniMock;

    private FeatureList.TestValues mParamsTestValues;
    private Context mContext;
    private @ModuleType int mExpectedModuleType;
    private EducationalTipModuleMediator mEducationalTipModuleMediator;

    @Before
    public void setUp() {
        mParamsTestValues = new FeatureList.TestValues();
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        when(mProfileSupplier.get()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mJniMocker.mock(
                EducationalTipModuleMediatorJni.TEST_HOOKS, mEducationalTipModuleMediatorJniMock);
        mExpectedModuleType = ModuleType.EDUCATIONAL_TIP;

        mEducationalTipModuleMediator =
                new EducationalTipModuleMediator(mModel, mModuleDelegate, mActionDelegate);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testShowModuleWithForcedCardType() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // Test showing default browser promo card.
        testShowModuleWithForcedCardTypeImpl(
                FORCE_DEFAULT_BROWSER,
                R.string.educational_tip_default_browser_title,
                R.string.educational_tip_default_browser_description,
                R.drawable.default_browser_promo_logo,
                /* timesOfCall= */ 1);

        // Test showing tab group promo card.
        testShowModuleWithForcedCardTypeImpl(
                FORCE_TAB_GROUP,
                R.string.educational_tip_tab_group_title,
                R.string.educational_tip_tab_group_description,
                R.drawable.tab_group_promo_logo,
                /* timesOfCall= */ 2);

        // Test showing tab group sync promo card.
        testShowModuleWithForcedCardTypeImpl(
                FORCE_TAB_GROUP_SYNC,
                R.string.educational_tip_tab_group_sync_title,
                R.string.educational_tip_tab_group_sync_description,
                R.drawable.tab_group_sync_promo_logo,
                /* timesOfCall= */ 3);

        // Test showing quick delete promo card.
        testShowModuleWithForcedCardTypeImpl(
                FORCE_QUICK_DELETE,
                R.string.educational_tip_quick_delete_title,
                R.string.educational_tip_quick_delete_description,
                R.drawable.quick_delete_promo_logo,
                /* timesOfCall= */ 4);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreateInputContext() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        InputContext inputContext = mEducationalTipModuleMediator.createInputContext();
        assertEquals(2, inputContext.getSizeForTesting());
        assertEquals(
                0, inputContext.getEntryForTesting("is_default_browser_chrome").floatValue, 0.01);
        assertEquals(
                1,
                inputContext.getEntryForTesting(
                                "has_default_browser_promo_reached_limit_in_role_manager")
                        .floatValue,
                0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreatePredictionOptions() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // Verifies that createPredictionOptions() returns ondemand prediction options.
        PredictionOptions actualOptions = mEducationalTipModuleMediator.createPredictionOptions();
        PredictionOptions expectedOptions = new PredictionOptions(/* onDemandExecution= */ true);
        actualOptions.equals(expectedOptions);
    }

    @Test
    @SmallTest
    public void testOnGetClassificationResult() {
        // Test when the segmentation result is invalid.
        ClassificationResult classificationResult =
                new ClassificationResult(
                        PredictionStatus.FAILED,
                        new String[] {
                            "default_browser_promo",
                            "tab_groups_promo",
                            "tab_group_sync_promo",
                            "quick_delete_promo"
                        });
        assertNull(mEducationalTipModuleMediator.onGetClassificationResult(classificationResult));

        // Test when the segmentation result is empty.
        classificationResult =
                new ClassificationResult(PredictionStatus.SUCCEEDED, new String[] {});
        assertNull(mEducationalTipModuleMediator.onGetClassificationResult(classificationResult));

        // Test when the segmentation result is valid and not empty.
        classificationResult =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {
                            "default_browser_promo",
                            "tab_groups_promo",
                            "tab_group_sync_promo",
                            "quick_delete_promo"
                        });
        Integer expectedResult = EducationalTipCardType.DEFAULT_BROWSER_PROMO;
        assertEquals(
                expectedResult,
                mEducationalTipModuleMediator.onGetClassificationResult(classificationResult));

        classificationResult =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {
                            "tab_groups_promo",
                            "default_browser_promo",
                            "tab_group_sync_promo",
                            "quick_delete_promo"
                        });
        expectedResult = EducationalTipCardType.TAB_GROUPS;
        assertEquals(
                expectedResult,
                mEducationalTipModuleMediator.onGetClassificationResult(classificationResult));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        mEducationalTipModuleMediator.showModuleWithCardInfo(
                EducationalTipCardType.DEFAULT_BROWSER_PROMO);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.DEFAULT_BROWSER_PROMO);

        mEducationalTipModuleMediator.showModuleWithCardInfo(EducationalTipCardType.TAB_GROUPS);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.TAB_GROUPS);
    }

    private void testShowModuleWithForcedCardTypeImpl(
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
        verify(mModuleDelegate, never()).onDataFetchFailed(mExpectedModuleType);

        mParamsTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.EDUCATIONAL_TIP_MODULE, enabledParam, "false");
    }
}
