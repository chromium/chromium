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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoTriggerStateListener;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.ClassificationResult;
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

    @Mock private PropertyModel mModel;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private ObservableSupplier<Profile> mProfileSupplier;
    @Mock private Profile mProfile;
    @Mock EducationalTipModuleMediator.Natives mEducationalTipModuleMediatorJniMock;
    @Mock private Tracker mTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;

    @Mock
    private EducationalTipCardProviderSignalHandler mMockEducationalTipCardProviderSignalHandler;

    @Captor
    private ArgumentCaptor<DefaultBrowserPromoTriggerStateListener>
            mDefaultBrowserPromoTriggerStateListener;

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
        EducationalTipModuleMediatorJni.setInstanceForTesting(mEducationalTipModuleMediatorJniMock);
        mExpectedModuleType = ModuleType.EDUCATIONAL_TIP;
        TrackerFactory.setTrackerForTests(mTracker);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        EducationalTipCardProviderSignalHandler.setInstanceForTesting(
                mMockEducationalTipCardProviderSignalHandler);

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
                            "tab_group_promo",
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
                            "tab_group_promo",
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
                            "tab_group_promo",
                            "default_browser_promo",
                            "tab_group_sync_promo",
                            "quick_delete_promo"
                        });
        expectedResult = EducationalTipCardType.TAB_GROUP;
        assertEquals(
                expectedResult,
                mEducationalTipModuleMediator.onGetClassificationResult(classificationResult));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_DefaultBrowserPromo() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // TODO(crbug.com/382803396): The sample here is a temporary workaround and will need to be
        // fully replaced with the module type after the refactor.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "MagicStack.Clank.NewTabPage.Module.EducationalTip.Impression",
                                EducationalTipCardType.DEFAULT_BROWSER_PROMO
                                        + ModuleType.NUM_ENTRIES,
                                EducationalTipCardType.TAB_GROUP + ModuleType.NUM_ENTRIES)
                        .build();

        when(mTracker.shouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);

        mEducationalTipModuleMediator.showModuleWithCardInfo(
                EducationalTipCardType.DEFAULT_BROWSER_PROMO);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.DEFAULT_BROWSER_PROMO);
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(
                        mEducationalTipModuleMediator
                                .getDefaultBrowserPromoTriggerStateListenerForTesting());
        verify(mMockDefaultBrowserPromoUtils).notifyDefaultBrowserPromoVisible();

        mEducationalTipModuleMediator.showModuleWithCardInfo(EducationalTipCardType.TAB_GROUP);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.DEFAULT_BROWSER_PROMO);
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(
                        mEducationalTipModuleMediator
                                .getDefaultBrowserPromoTriggerStateListenerForTesting());
        verify(mMockDefaultBrowserPromoUtils).notifyDefaultBrowserPromoVisible();

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testOnViewCreated_TabGroupPromo() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        // TODO(crbug.com/382803396): The sample here is a temporary workaround and will need to be
        // fully replaced with the module type after the refactor.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "MagicStack.Clank.NewTabPage.Module.EducationalTip.Impression",
                                EducationalTipCardType.TAB_GROUP + ModuleType.NUM_ENTRIES,
                                2)
                        .build();

        when(mMockEducationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP))
                .thenReturn(true);
        mEducationalTipModuleMediator.showModuleWithCardInfo(EducationalTipCardType.TAB_GROUP);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.TAB_GROUP);

        when(mMockEducationalTipCardProviderSignalHandler.shouldNotifyCardShownPerSession(
                        EducationalTipCardType.TAB_GROUP))
                .thenReturn(false);
        mEducationalTipModuleMediator.showModuleWithCardInfo(EducationalTipCardType.TAB_GROUP);
        mEducationalTipModuleMediator.onViewCreated();
        verify(mEducationalTipModuleMediatorJniMock)
                .notifyCardShown(mProfile, EducationalTipCardType.TAB_GROUP);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testRemoveModule() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        mEducationalTipModuleMediator.showModuleWithCardInfo(
                EducationalTipCardType.DEFAULT_BROWSER_PROMO);
        verify(mMockDefaultBrowserPromoUtils)
                .addListener(mDefaultBrowserPromoTriggerStateListener.capture());

        mDefaultBrowserPromoTriggerStateListener.getValue().onDefaultBrowserPromoTriggered();
        verify(mModuleDelegate).removeModule(mExpectedModuleType);
        verify(mMockDefaultBrowserPromoUtils)
                .removeListener(mDefaultBrowserPromoTriggerStateListener.capture());
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
