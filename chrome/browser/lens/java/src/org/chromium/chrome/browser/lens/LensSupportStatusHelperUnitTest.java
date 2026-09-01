// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.content.pm.PackageInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;

/** Unit tests for {@link LensSupportStatusHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.LENS_OVERLAY_ANDROID})
public class LensSupportStatusHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;

    @Before
    public void setUp() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        GSAUtils.setFakePassableGsaEnvironmentForTesting(false);
    }

    private void setGsaPackage(String versionName) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = GSAUtils.GSA_PACKAGE_NAME;
        packageInfo.versionName = versionName;
        packageInfo.applicationInfo = new android.content.pm.ApplicationInfo();
        packageInfo.applicationInfo.enabled = true;
        GSAUtils.setAgsaPackageInfoForTesting(packageInfo);
    }

    @Test
    public void getLensSupportStatus_incognito() {
        Assert.assertNull(
                "Status should be null in incognito",
                LensSupportStatusHelper.getLensSupportStatus(mProfile, true));
    }

    @Test
    public void getLensSupportStatus_nullProfile() {
        Assert.assertNull(
                "Status should be null when profile is null",
                LensSupportStatusHelper.getLensSupportStatus(null, false));
    }

    @Test
    public void getLensSupportStatus_nonGoogleDSE() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        Assert.assertEquals(
                "Status should be NON_GOOGLE_SEARCH_ENGINE",
                LensMetrics.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    public void getLensSupportStatus_noGsa() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        GSAUtils.setAgsaPackageInfoForTesting(null);
        Assert.assertEquals(
                "Status should be ACTIVITY_NOT_ACCESSIBLE",
                LensMetrics.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    public void getLensSupportStatus_outOfDateGsa() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        setGsaPackage("1.0"); // Very old version
        Assert.assertEquals(
                "Status should be OUT_OF_DATE",
                LensMetrics.LensSupportStatus.OUT_OF_DATE,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    public void getLensSupportStatus_supported() {
        GSAUtils.setFakePassableGsaEnvironmentForTesting(true);
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        // Version and package check are faked to pass
        Assert.assertEquals(
                "Status should be LENS_SEARCH_SUPPORTED",
                LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    public void isLensSearchSupported_supported() {
        GSAUtils.setFakePassableGsaEnvironmentForTesting(true);
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        Assert.assertTrue(LensSupportStatusHelper.isLensSearchSupported(mProfile, false));
    }

    @Test
    public void isLensSearchSupported_unsupported() {
        Assert.assertFalse(LensSupportStatusHelper.isLensSearchSupported(mProfile, true));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.LENS_OVERLAY_ANDROID
                    + ":"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_TYPE
                    + "/"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_WEBUI)
    public void getLensSupportStatus_webuiOverlay_skipGsaChecks() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        // Even if AGSA is not present, it should be supported
        GSAUtils.setAgsaPackageInfoForTesting(null);
        Assert.assertEquals(
                "Status should be LENS_SEARCH_SUPPORTED for WebUI implementation even without AGSA",
                LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void getLensSupportStatus_defaultOverlay_doGsaChecks() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        // AGSA is missing
        GSAUtils.setAgsaPackageInfoForTesting(null);
        Assert.assertEquals(
                "Status should be ACTIVITY_NOT_ACCESSIBLE for default implementation (intent)"
                        + " without AGSA",
                LensMetrics.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.LENS_OVERLAY_ANDROID
                    + ":"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_TYPE
                    + "/"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_INTENT)
    public void getLensSupportStatus_intentOverlay_doGsaChecks() {
        Mockito.when(mProfile.isOffTheRecord()).thenReturn(false);
        Mockito.when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        // AGSA is missing
        GSAUtils.setAgsaPackageInfoForTesting(null);
        Assert.assertEquals(
                "Status should be ACTIVITY_NOT_ACCESSIBLE for intent implementation without AGSA",
                LensMetrics.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE,
                (int) LensSupportStatusHelper.getLensSupportStatus(mProfile, false));
    }
}
