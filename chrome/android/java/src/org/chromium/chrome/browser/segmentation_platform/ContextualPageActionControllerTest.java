// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/**
 * Unit tests for {@link ContextualPageActionController}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
        ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, ChromeFeatureList.SHOPPING_LIST})
public class ContextualPageActionControllerTest {
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<Tab> mTabSupplier;

    @Mock
    private Profile mMockProfile;
    @Mock
    private Tab mMockTab;
    @Mock
    private ActivityLifecycleDispatcher mMockActivityLifecycleDispatcher;
    @Mock
    private Resources mMockResources;
    @Mock
    private Configuration mMockConfiguration;
    @Mock
    private AdaptiveToolbarButtonController mMockAdaptiveToolbarController;
    @Mock
    private ContextualPageActionController.Natives mMockControllerJni;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    ContextualPageActionController mContextualPageActionController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mProfileSupplier = new ObservableSupplierImpl<>();
        mTabSupplier = new ObservableSupplierImpl<>();

        mJniMocker.mock(ContextualPageActionControllerJni.TEST_HOOKS, mMockControllerJni);
        doReturn(mMockConfiguration).when(mMockResources).getConfiguration();
        doReturn(true).when(mMockActivityLifecycleDispatcher).isNativeInitializationFinished();
    }

    private ContextualPageActionController createContextualPageActionController() {
        ContextualPageActionController contextualPageActionController =
                new ContextualPageActionController(mProfileSupplier, mTabSupplier,
                        mMockActivityLifecycleDispatcher, mMockResources,
                        mMockAdaptiveToolbarController);

        mProfileSupplier.set(mMockProfile);

        return contextualPageActionController;
    }

    private void setMockSegmentationResult(
            org.chromium.components.segmentation_platform.proto.SegmentationProto
                    .SegmentId segmentationResult) {
        Mockito.doAnswer(invocation -> {
                   Callback<SegmentSelectionResult> callback = invocation.getArgument(2);
                   callback.onResult(new SegmentSelectionResult(true, segmentationResult));
                   return null;
               })
                .when(mMockControllerJni)
                .computeContextualPageAction(any(), any(), any());
    }

    @Test
    public void loadingTabsAreIgnored() {
        mMockConfiguration.screenWidthDp = 450;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);

        when(mMockTab.isLoading()).thenReturn(true);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());
    }

    @Test
    public void incognitoTabsRevertToDefaultAction() {
        mMockConfiguration.screenWidthDp = 450;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);

        when(mMockTab.isIncognito()).thenReturn(true);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);
    }

    @Test
    public void buttonNotShownWhenUiDisabled() {
        mMockConfiguration.screenWidthDp = 450;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS, "enable_ui", "false");
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, true);
        testValues.addFeatureFlagOverride(ChromeFeatureList.SHOPPING_LIST, true);
        FeatureList.setTestValues(testValues);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());
        // Even if the UI is disabled segmentation should be called.
        verify(mMockControllerJni).computeContextualPageAction(any(), any(), any());
    }

    @Test
    public void testButtonOnLargeScreens() {
        mMockConfiguration.screenWidthDp = 450;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.PRICE_TRACKING);
    }

    @Test
    public void testButtonNotShownOnSmallScreens() {
        mMockConfiguration.screenWidthDp = 320;

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);
        verify(mMockControllerJni, never()).computeContextualPageAction(any(), any(), any());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING})
    public void testConfigurationChangeIgnoredWhenFeatureIsDisabled() {
        mMockConfiguration.screenWidthDp = 600;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        // Set tab, no button should be shown because feature is disabled.
        mTabSupplier.set(mMockTab);

        // Change configuration, no button should be shown still.
        mMockConfiguration.screenWidthDp = 450;
        contextualPageActionController.onConfigurationChanged(mMockConfiguration);

        // Button should not be updated.
        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());
        // Segmentation should not be called.
        verify(mMockControllerJni, never()).computeContextualPageAction(any(), any(), any());
    }

    @Test
    public void testConfigurationChangeIgnoredWhenNativeNotReady() {
        mMockConfiguration.screenWidthDp = 600;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);
        doReturn(false).when(mMockActivityLifecycleDispatcher).isNativeInitializationFinished();

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        // Change configuration, button shouldn't update, and segmentation shouldn't be called.
        mMockConfiguration.screenWidthDp = 450;
        contextualPageActionController.onConfigurationChanged(mMockConfiguration);

        // Button should only be updated once when setting the tab.
        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.PRICE_TRACKING);
        // Segmentation should only be called once.
        verify(mMockControllerJni).computeContextualPageAction(any(), any(), any());
    }

    @Test
    public void testButtonShowsOnConfigurationChange() {
        // Screen is not wide enough to show button.
        mMockConfiguration.screenWidthDp = 320;
        setMockSegmentationResult(
                SegmentId.OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING);

        ContextualPageActionController contextualPageActionController =
                createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        // Screen is now wide enough to show button.
        mMockConfiguration.screenWidthDp = 600;
        contextualPageActionController.onConfigurationChanged(mMockConfiguration);

        // Return to original screen width.
        mMockConfiguration.screenWidthDp = 320;
        contextualPageActionController.onConfigurationChanged(mMockConfiguration);

        InOrder inOrder = inOrder(mMockAdaptiveToolbarController);

        inOrder.verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);
        inOrder.verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.PRICE_TRACKING);
        inOrder.verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);

        // Segmentation should only be called once.
        verify(mMockControllerJni).computeContextualPageAction(any(), any(), any());
    }
}
