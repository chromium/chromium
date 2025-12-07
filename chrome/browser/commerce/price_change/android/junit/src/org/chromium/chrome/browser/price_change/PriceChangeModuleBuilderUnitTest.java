// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link PriceChangeModuleBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class PriceChangeModuleBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ShoppingService mShoppingService;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mBuildCallback;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;

    private PriceChangeModuleBuilder mModuleBuilder;

    @Before
    public void setUp() {
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isPriceAnnotationsEnabled(anyLong());

        var profileProviderSupplier = new OneshotSupplierImpl<ProfileProvider>();
        profileProviderSupplier.set(mProfileProvider);
        mModuleBuilder =
                new PriceChangeModuleBuilder(
                        RuntimeEnvironment.application, profileProviderSupplier, mTabModelSelector);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @Test
    @SmallTest
    public void testBuildModule_NotEligible() {
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isPriceAnnotationsEnabled(anyLong());
        assertFalse(PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile));

        assertFalse(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, never()).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    public void testBuildModule_NotEligibleWithoutProfileProvider() {
        mModuleBuilder =
                new PriceChangeModuleBuilder(
                        RuntimeEnvironment.application,
                        new OneshotSupplierImpl<ProfileProvider>(),
                        mTabModelSelector);
        assertFalse(mModuleBuilder.isEligible());
    }

    @Test
    @SmallTest
    public void testBuildModule_NotEligibleWithoutProfile() {
        when(mProfileProvider.getOriginalProfile()).thenReturn(null);
        assertFalse(mModuleBuilder.isEligible());
    }

    @Test
    @SmallTest
    public void testBuildModule_Eligible() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
        assertTrue(PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile));

        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, times(1)).onResult(any(ModuleProvider.class));
    }
}
