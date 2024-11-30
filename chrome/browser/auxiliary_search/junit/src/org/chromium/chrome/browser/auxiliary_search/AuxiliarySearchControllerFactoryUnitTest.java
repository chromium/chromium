// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;

/** Unit tests for {@link AuxiliarySearchControllerFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchControllerFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock private FaviconHelper.Natives mMockFaviconHelperJni;

    private AuxiliarySearchControllerFactory mFactory;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mContext.getResources()).thenReturn(mResources);

        AuxiliarySearchBridgeJni.setInstanceForTesting(mMockAuxiliarySearchBridgeJni);
        when(mMockFaviconHelperJni.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mMockFaviconHelperJni);
        AuxiliarySearchDonor.setSkipInitializationForTesting(true);

        mFactory = AuxiliarySearchControllerFactory.getInstance();
    }

    @Test
    @SmallTest
    public void testIsEnabled() {
        mFactory.setHooksForTesting(null);
        assertFalse(mFactory.isEnabled());

        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(false);
        mFactory.setHooksForTesting(hooksMock);
        assertFalse(mFactory.isEnabled());

        when(hooksMock.isEnabled()).thenReturn(true);
        mFactory.setHooksForTesting(hooksMock);
        assertTrue(mFactory.isEnabled());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_V2)
    @Config(sdk = VERSION_CODES.Q)
    public void testCreateAuxiliarySearchController_LessThanS() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(false);
        mFactory.setHooksForTesting(hooksMock);
        assertFalse(mFactory.isEnabled());
        assertNull(mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector));

        when(hooksMock.isEnabled()).thenReturn(true);
        mFactory.setHooksForTesting(hooksMock);
        assertTrue(mFactory.isEnabled());
        mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector);
        verify(hooksMock)
                .createAuxiliarySearchController(eq(mContext), eq(mProfile), eq(mTabModelSelector));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_V2)
    @Config(sdk = VERSION_CODES.S)
    public void testCreateAuxiliarySearchController() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(false);
        mFactory.setHooksForTesting(hooksMock);
        assertFalse(mFactory.isEnabled());
        assertNull(mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector));

        when(hooksMock.isEnabled()).thenReturn(true);
        mFactory.setHooksForTesting(hooksMock);
        assertTrue(mFactory.isEnabled());
        when(mProfile.isOffTheRecord()).thenReturn(false);

        AuxiliarySearchController controller =
                mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector);
        assertTrue(controller instanceof AuxiliarySearchControllerImpl);
        verify(hooksMock, never())
                .createAuxiliarySearchController(eq(mContext), eq(mProfile), eq(mTabModelSelector));
    }

    @Test
    @SmallTest
    public void testIsSettingDefaultEnabledByOs() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(false);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        mFactory.setHooksForTesting(hooksMock);

        assertTrue(mFactory.isSettingDefaultEnabledByOs());

        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(mFactory.isSettingDefaultEnabledByOs());
    }
}
