// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.Manifest;
import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusMediator;
import org.chromium.chrome.browser.omnibox.status.StatusProperties;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Testing the interactions with permissions on a site and how it affects page info.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PageInfoDiscoverabilityTest {
    @Rule
    public PermissionTestRule mPermissionTestRule = new PermissionTestRule();

    private static final String GEOLOCATION_TEST =
            "/chrome/test/data/geolocation/geolocation_on_load.html";

    @Mock
    LocationBarDataProvider mLocationBarDataProvider;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    @Mock
    Runnable mMockForceModelViewReconciliationRunnable;
    @Mock
    SearchEngineLogoUtils mSearchEngineLogoUtils;
    @Mock
    Profile mProfile;
    @Mock
    TemplateUrlService mTemplateUrlService;

    Context mContext;
    Resources mResources;
    PropertyModel mModel;
    PermissionDialogController mPermissionDialogController;
    StatusMediator mMediator;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mPermissionTestRule.setUpActivity();
        mContext = mPermissionTestRule.getActivity();
        mResources = mContext.getResources();
        mModel = new PropertyModel(StatusProperties.ALL_KEYS);
        mPermissionDialogController = PermissionDialogController.getInstance();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator = new StatusMediator(mModel, mResources, mContext,
                    mUrlBarEditingTextStateProvider,
                    /* isTablet */ false, mMockForceModelViewReconciliationRunnable,
                    mLocationBarDataProvider, mPermissionDialogController, mSearchEngineLogoUtils,
                    ()
                            -> mTemplateUrlService,
                    () -> mProfile, null, mPermissionTestRule.getActivity().getWindowAndroid());
        });
    }

    @Test
    @MediumTest
    @Feature({"PageInfoDiscoverability"})
    @EnableFeatures({PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY})
    public void testPageInfoDiscoverabilityFlagOn() throws Exception {
        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());

        // Prompt for location and accept it.
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission = new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                Manifest.permission.ACCESS_FINE_LOCATION};
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(mPermissionTestRule, testAndroidPermissionDelegate,
                GEOLOCATION_TEST, true /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, null /* javascriptToExecute */,
                0 /* missingPermissionPromptTextId */);

        Assert.assertEquals(ContentSettingsType.GEOLOCATION, mMediator.getLastPermission());
    }

    @Test
    @MediumTest
    @Feature({"PageInfoDiscoverability"})
    @DisableFeatures({PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY})
    public void testPageInfoDiscoverabilityFlagOff() throws Exception {
        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());

        // Prompt for location and accept it.
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission = new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                Manifest.permission.ACCESS_FINE_LOCATION};
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testAndroidPermissionDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(requestablePermission,
                        RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(mPermissionTestRule, testAndroidPermissionDelegate,
                GEOLOCATION_TEST, true /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, null /* javascriptToExecute */,
                0 /* missingPermissionPromptTextId */);

        Assert.assertEquals(ContentSettingsType.DEFAULT, mMediator.getLastPermission());
    }
}