// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.ntp.LogoBridge;
import org.chromium.chrome.browser.ntp.LogoBridgeJni;
import org.chromium.chrome.browser.ntp.LogoDelegateImpl;
import org.chromium.chrome.browser.ntp.LogoView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for the {@link LogoLoadHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogoLoadHelperUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    Profile mMockProfile1;

    @Mock
    Profile mMockProfile2;

    @Mock
    LogoBridge.Natives mLogoBridge;

    @Mock
    TemplateUrlServiceFactory.Natives mTemplateUrlServiceFactory;

    @Mock
    TemplateUrlService mTemplateUrlService;

    @Mock
    LogoDelegateImpl mLogoDelegate;

    @Mock
    TopToolbarCoordinator mToolbar;

    private Context mContext;
    private LogoLoadHelper mLogoLoadHelper;
    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mProfileSupplier = new ObservableSupplierImpl<>();
        mContext = ApplicationProvider.getApplicationContext();
        mLogoLoadHelper = new LogoLoadHelper(mProfileSupplier, mToolbar, mContext);
        mLogoLoadHelper.setLogoDelegateForTesting(mLogoDelegate);

        doReturn(false).when(mMockProfile1).isOffTheRecord();
        doReturn(true).when(mMockProfile2).isOffTheRecord();

        mJniMocker.mock(TemplateUrlServiceFactoryJni.TEST_HOOKS, mTemplateUrlServiceFactory);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mTemplateUrlService).doesDefaultSearchEngineHaveLogo();

        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);

        CachedFeatureFlags.setForTesting(ChromeFeatureList.START_SURFACE_ANDROID, true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(true));
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @Test
    public void testDSEChangedOnRegularProfileAndGoogleIsDSE() {
        mProfileSupplier.set(mMockProfile1);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        Bitmap googleLogoBitmap = LogoView.getDefaultGoogleLogo(mContext.getResources());
        verify(mToolbar, times(1)).onLogoAvailable(eq(googleLogoBitmap), any());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedOnRegularProfileAndGoogleIsNotDSE() {
        mProfileSupplier.set(mMockProfile1);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
        // mToolbar#onLogoAvailable should also be called here, but it's a callback from
        // LogoObserver, which is hard to test. We check mLogoDelegate#getSearchProviderLogo
        // instead.
    }

    @Test
    public void testDSEChangedOnRegularProfileAndDoesNotHaveLogo() {
        mProfileSupplier.set(mMockProfile1);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        verify(mToolbar, times(1)).onLogoAvailable(null, null);
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testDSEChangedWithIncognitoProfile() {
        mProfileSupplier.set(mMockProfile2);

        mLogoLoadHelper.onDefaultSearchEngineChanged();

        verify(mToolbar, times(0)).onLogoAvailable(any(), any());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }

    @Test
    public void testLoadLogoOnStartSurfaceHomepageWhenLogoNotLoaded() {
        mProfileSupplier.set(mMockProfile1);
        mLogoLoadHelper.setHasLogoLoadedForCurrentSearchEngineForTesting(false);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        mLogoLoadHelper.maybeLoadSearchProviderLogoOnHomepage(StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mLogoDelegate, times(1)).getSearchProviderLogo(any());
        // mToolbar#onLogoAvailable should also be called here, but it's a callback from
        // LogoObserver, which is hard to test. We check mLogoDelegate#getSearchProviderLogo
        // instead.
    }

    @Test
    public void testLoadLogoOnStartSurfaceHomepageWhenLogoHasLoaded() {
        mProfileSupplier.set(mMockProfile1);
        mLogoLoadHelper.setHasLogoLoadedForCurrentSearchEngineForTesting(true);

        mLogoLoadHelper.maybeLoadSearchProviderLogoOnHomepage(StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mToolbar, times(0)).onLogoAvailable(any(), any());
        verify(mLogoDelegate, times(0)).getSearchProviderLogo(any());
    }
}
