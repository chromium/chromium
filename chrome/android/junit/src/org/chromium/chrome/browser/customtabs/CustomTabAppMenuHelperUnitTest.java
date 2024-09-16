// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIPHController;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;

/** Unit tests for {@link CustomTabAppMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabAppMenuHelperUnitTest {
    private static final String PACKAGE_NAME = "org.foo.bar";

    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Supplier<Profile> mProfileSupplier;
    @Mock private AppMenuCoordinator mAppMenuCoordinator;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Testing conditions other than version-specific/flag-controlled ones.
        CustomTabAppMenuHelper.setAppHistoryEnabledForTesting(true);
    }

    private CustomTabHistoryIPHController maybeCreateHistoryIPHController() {
        return CustomTabAppMenuHelper.maybeCreateHistoryIPHController(
                mAppMenuCoordinator,
                mActivity,
                mActivityTabProvider,
                mProfileSupplier,
                mIntentDataProvider);
    }

    @Test
    public void createIPH() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, true);
        when(mAppMenuCoordinator.getAppMenuHandler()).thenReturn(mAppMenuHandler);
        assertNull(maybeCreateHistoryIPHController());

        when(mIntentDataProvider.getClientPackageNameIdentitySharing()).thenReturn(PACKAGE_NAME);
        assertNotNull(maybeCreateHistoryIPHController());
    }

    @Test
    public void showHistoryItem() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
        assertFalse(
                "CCT History menu not hidden! package-name: false first-run: false",
                CustomTabAppMenuHelper.showHistoryItem(false, CustomTabsUiType.DEFAULT));
        assertFalse(
                "CCT History menu not hidden! package-name: true first-run: false",
                CustomTabAppMenuHelper.showHistoryItem(true, CustomTabsUiType.DEFAULT));

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, true);
        assertTrue(
                "CCT History menu hidden! package-name: true first-run: true",
                CustomTabAppMenuHelper.showHistoryItem(true, CustomTabsUiType.DEFAULT));
        assertFalse(
                "AuthTab history menu not hidden! package-name: true first-run: true",
                CustomTabAppMenuHelper.showHistoryItem(true, CustomTabsUiType.AUTH_TAB));
    }
}
