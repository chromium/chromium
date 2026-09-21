// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.widget.ImageView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/** Unit tests for {@link BottomBarHubColorMixerAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarHubColorMixerAdapterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubColorMixer mHubColorMixer;
    @Mock private Tab mRegularTab;
    @Mock private Tab mIncognitoTab;

    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private BottomBarView mBottomBarView;
    private SettableNonNullObservableSupplier<Boolean> mIsHidingSupplier;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private BottomBarHubColorMixerAdapter mAdapter;

    @Before
    public void setUp() {
        when(mRegularTab.isIncognito()).thenReturn(false);
        when(mIncognitoTab.isIncognito()).thenReturn(true);

        mIsHidingSupplier = ObservableSuppliers.createNonNull(false);
        mCurrentTabSupplier = ObservableSuppliers.createNullable();

        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        org.chromium.chrome.browser.ui.bottombar.R.layout
                                                .bottom_bar_layout,
                                        null,
                                        false);
        mBottomBarView.setNewTabBackgroundVisible(true);
        mBottomBarView.resetColors();
    }

    @After
    public void tearDown() {
        if (mAdapter != null) {
            mAdapter.destroy();
        }
        mActivityController.close();
    }

    private BottomBarHubColorMixerAdapter createAdapter() {
        return new BottomBarHubColorMixerAdapter(
                mBottomBarView, mHubColorMixer, mCurrentTabSupplier, mIsHidingSupplier);
    }

    @Test
    public void testRegisteredBlendsCount() {
        mAdapter = createAdapter();
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        assertEquals(5, captor.getAllValues().size());
    }

    @Test
    public void testInjectedRegistrationHelper() {
        HubColorMixerRegistrationHelper helper = mock(HubColorMixerRegistrationHelper.class);
        mAdapter =
                new BottomBarHubColorMixerAdapter(
                        mBottomBarView,
                        mHubColorMixer,
                        helper,
                        mCurrentTabSupplier,
                        mIsHidingSupplier);
        verify(helper, times(5)).registerBlend(any());
        verify(helper).setColorMixer(mHubColorMixer);

        mAdapter.destroy();
        verify(helper).destroy();
    }

    @Test
    public void testColorMixer_registeredBlendsUpdateColors() {
        mAdapter = createAdapter();
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        List<HubViewColorBlend> blends = captor.getAllValues();
        assertEquals(5, blends.size());

        // Update progress midway between DEFAULT and INCOGNITO
        for (HubViewColorBlend blend : blends) {
            blend.updateProgress(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO, 0.5f);
        }

        // 1. Verify background color is blended
        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedBg = ColorUtils.blendColorsMultiply(defaultBg, incognitoBg, 0.5f);

        ColorDrawable background = (ColorDrawable) mBottomBarView.getBackground();
        assertEquals(expectedBg, background.getColor());

        // 2. Verify New Tab bright surface tint is blended
        int defaultSurface =
                BottomBarUtils.getColorSurfaceBright(mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoSurface =
                BottomBarUtils.getColorSurfaceBright(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedSurface =
                ColorUtils.blendColorsMultiply(defaultSurface, incognitoSurface, 0.5f);
        assertEquals(expectedSurface, mBottomBarView.getNewTabBackgroundTintForTesting());

        // 3. Verify New Tab background ripple is blended
        int defaultRippleBg =
                BottomBarUtils.getRippleColorBackground(mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoRippleBg =
                BottomBarUtils.getRippleColorBackground(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedRippleBg =
                ColorUtils.blendColorsMultiply(defaultRippleBg, incognitoRippleBg, 0.5f);
        assertEquals(expectedRippleBg, mBottomBarView.getNewTabRippleBackgroundColorForTesting());

        // 4. Verify other ripples (and New Tab no-background ripple) are blended
        int defaultRippleNoBg =
                BottomBarUtils.getRippleColorNoBackground(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoRippleNoBg =
                BottomBarUtils.getRippleColorNoBackground(mActivity, BrandedColorScheme.INCOGNITO);
        int expectedRippleNoBg =
                ColorUtils.blendColorsMultiply(defaultRippleNoBg, incognitoRippleNoBg, 0.5f);
        assertEquals(expectedRippleNoBg, mBottomBarView.getOtherRipplesColorForTesting());

        // 5. Verify icon tint is blended
        int defaultOnSurface =
                IncognitoColors.getColorOnSurface(mActivity, /* isIncognito= */ false);
        int incognitoOnSurface =
                IncognitoColors.getColorOnSurface(mActivity, /* isIncognito= */ true);
        int expectedOnSurface =
                ColorUtils.blendColorsMultiply(defaultOnSurface, incognitoOnSurface, 0.5f);
        ColorStateList expectedIconTint =
                BottomBarUtils.getIconColorStateListFromOnSurface(mActivity, expectedOnSurface);
        ImageView newTabButton =
                mBottomBarView.findViewById(
                        org.chromium.chrome.browser.ui.bottombar.R.id.new_tab_button);
        assertEquals(
                expectedIconTint.getDefaultColor(),
                newTabButton.getImageTintList().getDefaultColor());

        // Verify discrete transition animator creation
        for (HubViewColorBlend blend : blends) {
            Animator anim =
                    blend.createAnimationForTransition(
                            HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO);
            assertNotNull(anim);
        }
    }

    @Test
    public void testDestroy_resetsToBaselineColorSchemeAndRemovesObservers() {
        mAdapter = createAdapter();
        ArgumentCaptor<HubViewColorBlend> captor = ArgumentCaptor.forClass(HubViewColorBlend.class);
        verify(mHubColorMixer, times(5)).registerBlend(captor.capture());
        List<HubViewColorBlend> blends = captor.getAllValues();
        for (HubViewColorBlend blend : blends) {
            blend.updateProgress(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO, 1.0f);
        }

        // Color was changed to incognito by progress
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // Calling destroy should unregister blends, remove observers, and reset to baseline
        // APP_DEFAULT
        mAdapter.destroy();

        verify(mHubColorMixer, times(5)).unregisterBlend(any());
        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());
        assertFalse(mIsHidingSupplier.hasObservers());
        assertFalse(mCurrentTabSupplier.hasObservers());
    }

    @Test
    public void testIsHiding_SyncsBottomBarViewColorScheme() {
        mCurrentTabSupplier.set(mIncognitoTab);
        mAdapter = createAdapter();

        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);

        // Initially not hiding -> APP_DEFAULT
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When hiding starts, incognito tab color scheme is synced immediately to BottomBarView
        mIsHidingSupplier.set(true);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // Switch to regular tab while hiding
        mCurrentTabSupplier.set(mRegularTab);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // Switch back to incognito tab while hiding
        mCurrentTabSupplier.set(mIncognitoTab);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When hiding completes (isHiding returns to false), the tab color scheme is retained
        mIsHidingSupplier.set(false);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When isHiding is false, setting currentTab does not update background color
        mCurrentTabSupplier.set(mRegularTab);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());
    }

    @Test
    public void testCurrentTabSupplier_WhenNotHiding_DoesNotUpdateColorScheme() {
        mCurrentTabSupplier.set(mIncognitoTab);
        mAdapter = createAdapter();

        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);

        // Initially not hiding -> APP_DEFAULT
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When not hiding, currentTab changes do not update BottomBarView color scheme
        mCurrentTabSupplier.set(mRegularTab);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When hiding starts, regular tab color scheme is synced (APP_DEFAULT)
        mIsHidingSupplier.set(true);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // When currentTab changes to incognito while hiding, color scheme updates to INCOGNITO
        mCurrentTabSupplier.set(mIncognitoTab);
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());
    }

    @Test
    public void testCurrentTabSupplier_WhenHidingWithNullTab_DefaultsToDefaultColorScheme() {
        mIsHidingSupplier.set(true);
        mCurrentTabSupplier.set(mIncognitoTab);
        mAdapter = createAdapter();

        int defaultBg =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        int incognitoBg =
                BottomBarUtils.getBottomBarBackgroundColor(mActivity, BrandedColorScheme.INCOGNITO);

        // Since hiding is true with incognito tab, scheme is INCOGNITO
        assertEquals(incognitoBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());

        // Setting tab to null while hiding defaults to APP_DEFAULT
        mCurrentTabSupplier.set(null);
        assertEquals(defaultBg, ((ColorDrawable) mBottomBarView.getBackground()).getColor());
    }
}
