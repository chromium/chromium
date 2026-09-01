// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.bottombar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomBarContainerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarContainerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Callback<Boolean> mRequestLayerUpdateCallback;
    @Mock private BottomControlsVisibilityController mVisibilityController;
    @Mock private Callback<Object> mOnModelTokenChange;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ActionRegistry mActionRegistry;
    @Mock private Profile mProfile;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LayoutStateProvider mLayoutStateProvider;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mGlicActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mAiModeActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mMenuActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createNullable();
    private OneshotSupplierImpl<String> mCountrySupplier;

    private Activity mActivity;
    private FrameLayout mBottomBarContainer;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private BottomBarContainerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("us");
        mTabSupplier.set(null);
        when(mActionRegistry.get(anyInt())).thenReturn(mActionSupplier);
        when(mActionRegistry.get(ActionId.GLIC)).thenReturn(mGlicActionSupplier);
        when(mActionRegistry.get(ActionId.AI_MODE)).thenReturn(mAiModeActionSupplier);
        when(mActionRegistry.get(ActionId.APP_MENU)).thenReturn(mMenuActionSupplier);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(new MenuUiState());
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        GlicEnabling.setEnabledForTesting(false);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mBottomBarContainer = new FrameLayout(mActivity);
                            mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(true);
                            mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
                            mModalDialogManagerSupplier =
                                    ObservableSuppliers.createNonNull(mModalDialogManager);
                            mProfileSupplier.set(mProfile);
                            mCoordinator =
                                    new BottomBarContainerCoordinator(
                                            mBottomBarContainer,
                                            mRequestLayerUpdateCallback,
                                            mActionRegistry,
                                            mTabSupplier,
                                            mThemeColorProvider,
                                            mHomepageEnabledSupplier,
                                            mProfileSupplier,
                                            mCountrySupplier,
                                            mOmniboxFocusStateSupplier,
                                            mModalDialogManagerSupplier,
                                            new OneshotSupplierImpl<AppMenuCoordinator>(),
                                            mLayoutStateProvider);
                        });
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testInitialization() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mVisibilityController).setBottomControlsVisible(true);
        verify(mOnModelTokenChange).onResult(any());
        verify(mActionRegistry, times(2)).get(ActionId.NEW_TAB);
    }

    @Test
    public void testGetScrollBehavior() {
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, mCoordinator.getScrollBehavior());
    }

    @Test
    public void testGetBackgroundColor() {
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                (Integer)
                        BottomBarUtils.getBottomBarBackgroundColor(
                                mActivity, BrandedColorScheme.APP_DEFAULT),
                mCoordinator.getBackgroundColor());
    }

    @Test
    public void testGetBottomBar() {
        BottomBar bottomBar = mCoordinator.getBottomBar();
        assertNotNull(bottomBar);

        View view = bottomBar.getView();
        assertNotNull(view);

        bottomBar.setParent(Host.HUB); // Should not crash
    }

    @Test
    public void testAttachBottomBarView_notInitialized() {
        View childView = new View(mActivity);
        mCoordinator.attachBottomBarView(childView);
        assertEquals(1, mBottomBarContainer.getChildCount());
        assertEquals(childView, mBottomBarContainer.getChildAt(0));
        verify(mRequestLayerUpdateCallback).onResult(true);
        verify(mOnModelTokenChange, never()).onResult(any());
    }

    @Test
    public void testOnVisibilityChanged() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        mCoordinator.onVisibilityChanged(false);
        assertEquals(View.GONE, mBottomBarContainer.getVisibility());
        verify(mVisibilityController).setBottomControlsVisible(false);

        mCoordinator.onVisibilityChanged(true);
        assertEquals(View.VISIBLE, mBottomBarContainer.getVisibility());
        verify(mVisibilityController, times(2)).setBottomControlsVisible(true);
    }

    @Test
    public void testAttachBottomBarView_initialized() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        View childView = new View(mActivity);
        mCoordinator.attachBottomBarView(childView);
        assertEquals(1, mBottomBarContainer.getChildCount());
        assertEquals(childView, mBottomBarContainer.getChildAt(0));
        verify(mRequestLayerUpdateCallback).onResult(true);
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }

    @Test
    public void testOnConfigurationChanged() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig = new Configuration();
        newConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig);

        // Runnable is posted, verify it hasn't run yet.
        verify(mOnModelTokenChange, times(1)).onResult(any());

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable executed.
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }

    @Test
    public void testOnConfigurationChanged_debounce() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig1 = new Configuration();
        newConfig1.orientation = Configuration.ORIENTATION_LANDSCAPE;

        Configuration newConfig2 = new Configuration();
        newConfig2.orientation = Configuration.ORIENTATION_PORTRAIT;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig1);
        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig2);

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable executed only once more (total 2).
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }

    @Test
    public void testOnConfigurationChanged_sameOrientation() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig = new Configuration();
        int currentOrientation = mActivity.getResources().getConfiguration().orientation;
        newConfig.orientation = currentOrientation;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig);

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable NOT executed.
        verify(mOnModelTokenChange, times(1)).onResult(any());
    }

    @Test
    public void testOnBackgroundColorChanged() {
        mCoordinator.onBackgroundColorChanged();
        verify(mRequestLayerUpdateCallback).onResult(false);
    }

    @Test
    public void testCountrySupplier_DelayedSupply_BindsExtraButtonWhenAvailable() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        OneshotSupplierImpl<String> countrySupplier = new OneshotSupplierImpl<>();
        mCoordinator.destroy();

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .build();
        mGlicActionSupplier.set(glicModel);

        mCoordinator =
                new BottomBarContainerCoordinator(
                        mBottomBarContainer,
                        mRequestLayerUpdateCallback,
                        mActionRegistry,
                        mTabSupplier,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mProfileSupplier,
                        countrySupplier,
                        mOmniboxFocusStateSupplier,
                        mModalDialogManagerSupplier,
                        new OneshotSupplierImpl<AppMenuCoordinator>(),
                        mLayoutStateProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNull(extraButton);

        countrySupplier.set("us");
        RobolectricUtil.runAllBackgroundAndUi();

        extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNotNull(extraButton);
        assertEquals(View.VISIBLE, extraButton.getVisibility());
        assertEquals("Ask Gemini", extraButton.getContentDescription());
    }

    @Test
    public void testCountrySupplier_EmptyCountry_FailsClosedAndRemainsHidden() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        OneshotSupplierImpl<String> countrySupplier = new OneshotSupplierImpl<>();
        mCoordinator.destroy();

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .build();
        mGlicActionSupplier.set(glicModel);

        mCoordinator =
                new BottomBarContainerCoordinator(
                        mBottomBarContainer,
                        mRequestLayerUpdateCallback,
                        mActionRegistry,
                        mTabSupplier,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mProfileSupplier,
                        countrySupplier,
                        mOmniboxFocusStateSupplier,
                        mModalDialogManagerSupplier,
                        new OneshotSupplierImpl<AppMenuCoordinator>(),
                        mLayoutStateProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        countrySupplier.set("");
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNull(extraButton);
        View extraContainer =
                mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button_container);
        assertNotNull(extraContainer);
        assertEquals(View.GONE, extraContainer.getVisibility());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_aim_geofencing/true"
    })
    public void testCountrySupplier_AuCountry_BindsAiModeExtraButton() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        OneshotSupplierImpl<String> countrySupplier = new OneshotSupplierImpl<>();
        mCoordinator.destroy();

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .build();
        PropertyModel aiModeModel =
                new PropertyModel.Builder(ActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask AI Mode")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask AI Mode")
                        .build();
        mGlicActionSupplier.set(glicModel);
        mAiModeActionSupplier.set(aiModeModel);

        mCoordinator =
                new BottomBarContainerCoordinator(
                        mBottomBarContainer,
                        mRequestLayerUpdateCallback,
                        mActionRegistry,
                        mTabSupplier,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mProfileSupplier,
                        countrySupplier,
                        mOmniboxFocusStateSupplier,
                        mModalDialogManagerSupplier,
                        new OneshotSupplierImpl<AppMenuCoordinator>(),
                        mLayoutStateProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNull(extraButton);

        // "au" is in AIM_ALLOWED_COUNTRIES, but not GLIC_ALLOWED_COUNTRIES.
        countrySupplier.set("au");
        RobolectricUtil.runAllBackgroundAndUi();

        extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNotNull(extraButton);
        assertEquals(View.VISIBLE, extraButton.getVisibility());
        assertEquals("Ask AI Mode", extraButton.getContentDescription());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM)
    public void testCountrySupplier_FrCountry_FailsClosedAndRemainsHidden() {
        GlicEnabling.setEnabledForTesting(/* isEnabled= */ true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        OneshotSupplierImpl<String> countrySupplier = new OneshotSupplierImpl<>();
        mCoordinator.destroy();

        PropertyModel glicModel =
                new PropertyModel.Builder(GlicActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask Gemini")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask Gemini")
                        .build();
        PropertyModel aiModeModel =
                new PropertyModel.Builder(ActionProperties.ALL_KEYS)
                        .with(
                                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                                context -> "Ask AI Mode")
                        .with(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Ask AI Mode")
                        .build();
        mGlicActionSupplier.set(glicModel);
        mAiModeActionSupplier.set(aiModeModel);

        mCoordinator =
                new BottomBarContainerCoordinator(
                        mBottomBarContainer,
                        mRequestLayerUpdateCallback,
                        mActionRegistry,
                        mTabSupplier,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mProfileSupplier,
                        countrySupplier,
                        mOmniboxFocusStateSupplier,
                        mModalDialogManagerSupplier,
                        new OneshotSupplierImpl<AppMenuCoordinator>(),
                        mLayoutStateProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        // "fr" is not in GLIC_ALLOWED_COUNTRIES and not in AIM_ALLOWED_COUNTRIES.
        countrySupplier.set("fr");
        RobolectricUtil.runAllBackgroundAndUi();

        View extraButton = mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button);
        assertNull(extraButton);
        View extraContainer =
                mCoordinator.getBottomBar().getView().findViewById(R.id.extra_button_container);
        assertNotNull(extraContainer);
        assertEquals(View.GONE, extraContainer.getVisibility());
    }
}
