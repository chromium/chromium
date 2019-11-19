// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.DEFAULT_TIMEOUT_MS;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER;
import static org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationUnitTestRule.PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER;

import android.net.Uri;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for the partner disabling bookmarks editing functionality.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PartnerDisableBookmarksEditingUnitTest {
    @Rule
    public BasePartnerBrowserCustomizationUnitTestRule mTestRule =
            new BasePartnerBrowserCustomizationUnitTestRule();

    private void setBookmarksEditingDisabled(boolean disabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.BOOKMARKS_EDITING_DISABLED_KEY, disabled);
        mTestRule.getContextWrapper().getContentResolver().call(
                uri, "setBookmarksEditingDisabled", null, bundle);
    }

    @Before
    public void setUp() {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testProviderNotFromSystemPackage() throws InterruptedException {
        // Note that unlike other tests in this file, we test if Chrome ignores a customizations
        // provider that is not from a system package.
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(false);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PartnerBrowserCustomizations.initializeAsync(
                    mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(PartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testNoProvider() throws InterruptedException {
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PartnerBrowserCustomizations.initializeAsync(
                    mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);
        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(PartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingNotDisabled() throws InterruptedException {
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PartnerBrowserCustomizations.initializeAsync(
                    mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(PartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingDisabled() throws InterruptedException {
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PartnerBrowserCustomizations.initializeAsync(
                    mTestRule.getContextWrapper(), DEFAULT_TIMEOUT_MS);
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                mTestRule.getCallback(), DEFAULT_TIMEOUT_MS);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(PartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Test
    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingProviderDelayed() throws InterruptedException {
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        mTestRule.setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        setBookmarksEditingDisabled(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PartnerBrowserCustomizations.initializeAsync(mTestRule.getContextWrapper(), 2000);
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback());

        Assert.assertFalse(PartnerBrowserCustomizations.isInitialized());
        Assert.assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());

        TestPartnerBrowserCustomizationsDelayedProvider.unblockQuery();
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mTestRule.getCallback(), 3000);

        mTestRule.getCallbackLock().acquire();

        Assert.assertTrue(PartnerBrowserCustomizations.isInitialized());
        Assert.assertTrue(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }
}
