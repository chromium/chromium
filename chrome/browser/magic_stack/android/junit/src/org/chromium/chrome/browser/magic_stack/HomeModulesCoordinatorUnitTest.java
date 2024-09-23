// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.CirclePagerIndicatorDecoration.getItemPerScreen;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.HashSet;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {HomeModulesCoordinatorUnitTest.ShadowSemanticColorUtils.class})
public class HomeModulesCoordinatorUnitTest {
    @Implements(SemanticColorUtils.class)
    static class ShadowSemanticColorUtils {
        @Implementation
        public static int getDefaultIconColorSecondary(Context context) {
            return Color.LTGRAY;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Resources mResources;
    @Mock private ModuleDelegateHost mModuleDelegateHost;
    @Mock private ViewGroup mView;
    @Mock private HomeModulesRecyclerView mRecyclerView;
    @Mock private UiConfig mUiConfig;
    @Mock private Configuration mConfiguration;
    @Mock private ApplicationInfo mApplicationInfo;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private ObservableSupplierImpl<Profile> mProfileSupplier;
    @Mock private Profile mProfile;
    @Mock SegmentationPlatformService mSegmentationPlatformService;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private HomeModulesMediator mMediator;
    @Mock private ModelList mModel;

    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserver;
    @Captor private ArgumentCaptor<Callback<Profile>> mProfileObserver;
    @Captor private ArgumentCaptor<RecyclerView.OnScrollListener> mOnScrollListener;
    @Captor private ArgumentCaptor<Callback<ClassificationResult>> mClassificationResultCaptor;

    @Captor
    private ArgumentCaptor<HomeModulesConfigManager.HomeModulesStateListener>
            mHomeModulesStateListener;

    private HomeModulesCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mModuleDelegateHost.getUiConfig()).thenReturn(mUiConfig);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);
        when(mActivity.getApplicationInfo()).thenReturn(mApplicationInfo);
        when(mView.findViewById(R.id.home_modules_recycler_view)).thenReturn(mRecyclerView);
        when(mRecyclerView.getContext()).thenReturn(mActivity);
        when(mHomeModulesConfigManager.getEnabledModuleSet())
                .thenReturn(new HashSet<>(Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB)));
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        SegmentationPlatformServiceFactory.setForTests(mSegmentationPlatformService);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER, true);
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2, true);
        FeatureList.setTestValues(testValues);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        FeatureList.setTestValues(null);
    }

    @Test
    @SmallTest
    public void testCreate_phones() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        // Verifies that there isn't an observer of UiConfig registered.
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
        verify(mUiConfig, never()).addObserver(mDisplayStyleObserver.capture());
    }

    @Test
    @SmallTest
    public void testCreate_tablets() {
        setupAndVerifyTablets();
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());

        // Verifies that the snap scroll helper isn't attached to the recyclerview if there are more
        // than one item shown per screen.
        assertEquals(2, getItemPerScreen(displayStyle));
        assertFalse(mCoordinator.getIsSnapHelperAttachedForTesting());

        // Verifies that the snap scroll helper is attached to the recyclerview if there is only one
        // item shown per screen.
        DisplayStyle newDisplayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        mDisplayStyleObserver.getValue().onDisplayStyleChanged(newDisplayStyle);

        assertEquals(1, getItemPerScreen(newDisplayStyle));
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
    }

    @Test
    @SmallTest
    public void testHide() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        verify(mRecyclerView).setAdapter(notNull());

        mCoordinator.hide();
        verify(mRecyclerView).setAdapter(eq(null));

        showWithSegmentation((isVisible) -> {});
        verify(mRecyclerView, times(2)).setAdapter(notNull());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        setupAndVerifyTablets();
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());

        mCoordinator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserver.capture());
        assertNull(mCoordinator.getHomeModulesContextMenuManagerForTesting());
    }

    @Test
    @SmallTest
    public void testOnModuleConfigChanged() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        verify(mHomeModulesConfigManager).addListener(mHomeModulesStateListener.capture());
        Set<Integer> expectedModuleListBeforeHidingModule =
                Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, false);
        Set<Integer> expectedModuleListAfterHidingModule = Set.of(ModuleType.SINGLE_TAB);
        assertEquals(
                expectedModuleListAfterHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mCoordinator.destroy();
        verify(mHomeModulesConfigManager).removeListener(mHomeModulesStateListener.capture());
    }

    @Test
    @SmallTest
    public void testRemoveModuleAndDisable() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        mCoordinator.removeModuleAndDisable(ModuleType.PRICE_CHANGE);
        verify(mHomeModulesConfigManager)
                .setPrefModuleTypeEnabled(eq(ModuleType.PRICE_CHANGE), eq(false));
    }

    @Test
    @SmallTest
    public void testProfileNotReady() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        mCoordinator.show(callback);

        verify(mProfileSupplier).addObserver(mProfileObserver.capture());
        when(mProfileSupplier.hasValue()).thenReturn(true);
        mProfileObserver.getValue().onResult(mProfile);

        verify(mProfileSupplier).removeObserver(mProfileObserver.capture());
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
    })
    public void testRecordMagicStackScroll_Scrolled() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        mCoordinator.setMediatorForTesting(mMediator);

        mCoordinator.prepareBuildAndShow();

        // Besides the onScrollListener added in {@link HomeModulesCoordinator}, there is another
        // one added in {@link SnapHelper}.
        verify(mRecyclerView, times(2)).addOnScrollListener(mOnScrollListener.capture());
        mOnScrollListener.getAllValues().get(0).onScrolled(mRecyclerView, 1, 0);
        mOnScrollListener.getAllValues().get(1).onScrolled(mRecyclerView, 1, 0);

        verify(mMediator).recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ true);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
    })
    public void testRecordMagicStackScroll_NotScrolled() {
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.show(callback);

        mCoordinator.destroy();

        verify(mMediator).recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ false);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
    })
    public void testOnModuleChangedCallback() {
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        Callback<Boolean> onHomeModulesShownCallback = Mockito.mock(Callback.class);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.setModelForTesting(mModel);

        Runnable onHomeModulesChangedCallback =
                mCoordinator.createOnModuleChangedCallback(onHomeModulesShownCallback);
        Mockito.clearInvocations(mRecyclerView);

        when(mModel.size()).thenReturn(2);
        onHomeModulesChangedCallback.run();
        verify(mRecyclerView).invalidateItemDecorations();

        when(mModel.size()).thenReturn(1);
        onHomeModulesChangedCallback.run();
        verify(onHomeModulesShownCallback).onResult(true);

        when(mModel.size()).thenReturn(0);
        onHomeModulesChangedCallback.run();
        verify(onHomeModulesShownCallback).onResult(false);
    }

    private void setupAndVerifyTablets() {
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET);
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
    }

    private HomeModulesCoordinator createCoordinator(boolean skipInitProfile) {
        if (!skipInitProfile) {
            when(mProfileSupplier.hasValue()).thenReturn(true);
            when(mProfileSupplier.get()).thenReturn(mProfile);
        }
        HomeModulesCoordinator homeModulesCoordinator =
                new HomeModulesCoordinator(
                        mActivity,
                        mModuleDelegateHost,
                        mView,
                        mHomeModulesConfigManager,
                        mProfileSupplier,
                        mModuleRegistry);
        return homeModulesCoordinator;
    }

    private void showWithSegmentation(Callback<Boolean> callback) {
        mCoordinator.show(callback);
        verify(mSegmentationPlatformService)
                .getClassificationResult(
                        anyString(), any(), any(), mClassificationResultCaptor.capture());
        ClassificationResult result = new ClassificationResult(PredictionStatus.FAILED, null);
        mClassificationResultCaptor.getValue().onResult(result);
    }
}
