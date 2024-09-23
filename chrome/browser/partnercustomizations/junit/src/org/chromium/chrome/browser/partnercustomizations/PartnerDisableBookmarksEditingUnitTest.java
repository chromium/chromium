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

/** Unit tests for the partner disabling bookmarks editing functionality. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PartnerDisableBookmarksEditingUnitTest {
    @Rule
    public BasePartnerBrowserCustomizationUnitTestRule mTestRule =
            new BasePartnerBrowserCustomizationUnitTestRule();

    private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    private void setBookmarksEditingDisabled(boolean disabled) {
        Uri uri =
                CustomizationProviderDelegateUpstreamImpl.buildQueryUri(
                        PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.BOOKMARKS_EDITING_DISABLED_KEY, disabled);
        mTestRule
                .getContextWrapper()
                .getContentResolver()
                .call(uri, "setBookmarksEditingDisabled", null, bundle);
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
    @Feature({"PartnerBookmarksEditing"})
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
        Assert.assertFalse(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
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
        Assert.assertFalse(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingNotDisabled() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingDisabled() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    @DisabledTest(message = "Flaky due to ConcurrentModificationException, crbug.com/1446093")
    public void testBookmarksEditingProviderDelayed() throws InterruptedException {
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        mTestRule.setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        setBookmarksEditingDisabled(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPartnerBrowserCustomizations.initializeAsync(
                            mTestRule.getContextWrapper(), 2000);
                });
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback());

        Assert.assertFalse(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());

        TestPartnerBrowserCustomizationsDelayedProvider.unblockQuery();
        mPartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 3000);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(mPartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(mPartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }
}
