// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.DEFAULT_TIMEOUT_MS;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.common.ChromeUrlConstants;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.url.GURL;

/** Unit test suite for partner homepage. */
@DoNotBatch(reason = "Testing tests start up and homepage loading.")
@RunWith(ChromeJUnit4ClassRunner.class)
public class PartnerHomepageUnitTest {

    @Rule
    public BasePartnerBrowserCustomizationUnitTestRule mTestRule =
            new BasePartnerBrowserCustomizationUnitTestRule();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    public static final String TAG = "PartnerHomepageUnitTest";

    private static final GURL TEST_CUSTOM_HOMEPAGE_GURL = new GURL("http://chrome.com");

    private HomepageManager mHomepageManager;
    private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager = HomepageManager.getInstance();
                    mPartnerBrowserCustomizations = PartnerBrowserCustomizations.getInstance();
                });

        Assert.assertNotNull(mHomepageManager);

        Assert.assertNotSame(
                TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                TEST_CUSTOM_HOMEPAGE_GURL.getSpec());
        CustomizationProviderDelegateUpstreamImpl.ignoreBrowserProviderSystemPackageCheckForTesting(
                true);
    }

    @After
    public void tearDown() {
        PartnerBrowserCustomizations.destroy();
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testDefaultHomepage() {
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        assertHomePageIsNtp();
    }

    /** Everything is enabled for using partner homepage, except that there is no flag file. */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testProviderNotFromSystemPackage() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

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
        Assert.assertFalse(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        assertHomePageIsNtp();
    }

    /**
     * Everything is enabled for using partner homepage, except that there is no actual provider.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testNoProvider() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

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
        Assert.assertFalse(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        assertHomePageIsNtp();
    }

    /**
     * Everything is enabled for using partner homepage, except that the homepage preference is
     * disabled.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testHomepageDisabled() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(false);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

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
        Assert.assertTrue(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                mPartnerBrowserCustomizations.getHomePageUrl().getSpec());
        Assert.assertFalse(mHomepageManager.isHomepageEnabled());
        Assert.assertTrue(mHomepageManager.getHomepageGurl().isEmpty());
    }

    /**
     * Everything is enabled for using partner homepage, except that the preference is set to use
     * custom user-specified homepage.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testCustomHomepage() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(
                            false, false, TEST_CUSTOM_HOMEPAGE_GURL);
                });

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
        Assert.assertTrue(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                mPartnerBrowserCustomizations.getHomePageUrl().getSpec());
        Assert.assertTrue(mHomepageManager.isHomepageEnabled());
        Assert.assertEquals(TEST_CUSTOM_HOMEPAGE_GURL, mHomepageManager.getHomepageGurl());
    }

    /**
     * Everything is enabled for using partner homepage, but the homepage provider query takes
     * longer than the timeout we specify.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/837311")
    @Feature({"Homepage"})
    public void testHomepageProviderTimeout() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), 500);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 300);

        mTestRule.getCallbackLock().acquire();

        Assert.assertFalse(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        Assert.assertFalse(mHomepageManager.isHomepageEnabled());
        Assert.assertTrue(mHomepageManager.getHomepageGurl().isEmpty());

        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 2000);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        assertHomePageIsNtp();
    }

    /**
     * Everything is enabled for using partner homepage. The homepage provider query does not take
     * longer than the timeout we specify, but longer than the first async task wait timeout. This
     * scenario covers that the homepage provider is not ready at the cold startup initial homepage
     * tab, but be ready later than that.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    @DisabledTest(message = "crbug.com/837130")
    public void testHomepageProviderDelayed() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        mTestRule.setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_HOMEPAGE_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), 2000);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 300);

        mTestRule.getCallbackLock().acquire();

        Assert.assertFalse(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertNull(mPartnerBrowserCustomizations.getHomePageUrl());
        Assert.assertFalse(mHomepageManager.isHomepageEnabled());
        Assert.assertTrue(mHomepageManager.getHomepageGurl().isEmpty());

        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 3000);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsDelayedProvider.HOMEPAGE_URI,
                mPartnerBrowserCustomizations.getHomePageUrl().getSpec());
        Assert.assertTrue(mHomepageManager.isHomepageEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsDelayedProvider.HOMEPAGE_URI,
                mHomepageManager.getHomepageGurl().getSpec());
    }

    /**
     * Everything is enabled for using partner homepage. It should be able to successfully retrieve
     * homepage URI from the provider.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testReadHomepageProvider() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHomepageManager.setPrefHomepageEnabled(true);
                    mHomepageManager.setHomepagePreferences(false, true, TEST_CUSTOM_HOMEPAGE_GURL);
                });

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
        Assert.assertTrue(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                mPartnerBrowserCustomizations.getHomePageUrl().getSpec());
        Assert.assertTrue(mHomepageManager.isHomepageEnabled());
        Assert.assertEquals(
                TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                mHomepageManager.getHomepageGurl().getSpec());
    }

    private void assertHomePageIsNtp() {
        // The home page should default to the NTP
        Assert.assertTrue(mHomepageManager.isHomepageEnabled());
        Assert.assertEquals(ChromeUrlConstants.nativeNtpGurl(), mHomepageManager.getHomepageGurl());
    }
}
