// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link PriceChangeModuleBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class PriceChangeModuleBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ShoppingService mShoppingService;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mBuildCallback;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;

    private PriceChangeModuleBuilder mModuleBuilder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        mModuleBuilder =
                new PriceChangeModuleBuilder(
                        RuntimeEnvironment.application,
                        new ObservableSupplier<>() {
                            @Override
                            public Profile addObserver(Callback<Profile> obs) {
                                return null;
                            }

                            @Override
                            public void removeObserver(Callback<Profile> obs) {}

                            @Override
                            public Profile get() {
                                return mProfile;
                            }
                        },
                        mTabModelSelector);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @Test
    @SmallTest
    public void testBuildModule_NotEligible() {
        assertFalse(PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile));

        assertFalse(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, never()).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    public void testBuildModule_NotEligibleWithoutProfileInitialized() {
        mProfile = null;
        assertFalse(mModuleBuilder.isEligible());
    }

    @Test
    @SmallTest
    public void testBuildModule_Eligible() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        assertTrue(PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile));

        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, times(1)).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    public void testGetRegularProfile() {
        Profile regularProfile = Mockito.mock(Profile.class);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(regularProfile);

        assertEquals(regularProfile, mModuleBuilder.getRegularProfile());
    }
}
