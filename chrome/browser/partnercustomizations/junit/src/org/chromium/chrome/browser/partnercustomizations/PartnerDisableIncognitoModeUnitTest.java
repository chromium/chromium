// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.DEFAULT_TIMEOUT_MS;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER;

import android.net.Uri;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/** Unit tests for the partner disabling incognito mode functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PartnerDisableIncognitoModeUnitTest {
    @Rule
    public BasePartnerBrowserCustomizationUnitTestRule mTestRule =
            new BasePartnerBrowserCustomizationUnitTestRule();

    private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri =
                CustomizationProviderDelegateUpstreamImpl.buildQueryUri(
                        PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        mTestRule
                .getContextWrapper()
                .getContentResolver()
                .call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    @Before
    public void setUp() {
        CustomizationProviderDelegateUpstreamImpl.ignoreBrowserProviderSystemPackageCheckForTesting(
                true);
        mPartnerBrowserCustomizations = PartnerBrowserCustomizations.getInstance();
    }

    @After
    public void tearDown() {
        PartnerBrowserCustomizations.destroy();
    }

    @Test
    @SmallTest
    @Feature({"ParentalControls"})
    public void testProviderNotFromSystemPackage() throws InterruptedException {
        // Note that unlike other tests in this file, we test if Chrome ignores a customizations
        // provider that is not from a system package.
        CustomizationProviderDelegateUpstreamImpl.ignoreBrowserProviderSystemPackageCheckForTesting(
                false);
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @Test
    @SmallTest
    @Feature({"ParentalControls"})
    public void testNoProvider() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);
        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @Test
    @SmallTest
    @Feature({"ParentalControls"})
    public void testParentalControlsNotEnabled() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setParentalControlsEnabled(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @Test
    @SmallTest
    @Feature({"ParentalControls"})
    public void testParentalControlsEnabled() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setParentalControlsEnabled(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @Test
    @SmallTest
    @Feature({"ParentalControls"})
    @DisabledTest(message = "https://crbug.com/1446093")
    public void testParentalControlsProviderDelayed() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        mTestRule.setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        setParentalControlsEnabled(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), 2000);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback());

        Assert.assertFalse(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());

        TestPartnerBrowserCustomizationsDelayedProvider.unblockQuery();
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 3000);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(PartnerBrowserCustomizations.isIncognitoDisabled());
    }
}
