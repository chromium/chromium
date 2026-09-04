// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

/** Instrumentation tests for {@link TwaValidator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TwaValidatorTest {
    // Package from test support APK (contains TrustedWebActivityService implementation)
    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";
    private static final String NON_TWA_PACKAGE = "com.android.settings";
    private static final String TEST_URL = "https://www.example.com/app";
    private static final Origin TEST_ORIGIN = Origin.create(TEST_URL);
    private static final Origin UNVERIFIED_ORIGIN =
            Origin.create("https://www.unverified-domain.com");

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        new InstalledWebappPermissionStore().clearForTesting();
    }

    @After
    public void tearDown() {
        new InstalledWebappPermissionStore().clearForTesting();
    }

    @Test
    @MediumTest
    public void testIsTwaForOrigin_pureNativeAppNotTwa() {
        assertFalse(TwaValidator.isTwaForOrigin(mContext, NON_TWA_PACKAGE, TEST_ORIGIN));
    }

    @Test
    @MediumTest
    public void testIsTwaForOrigin_declaresServiceWithoutVerification_notTwa() {
        // TEST_SUPPORT_PACKAGE declares TrustedWebActivityService, but doesn't have DAL
        // verification or DomainManagerVerification for `UNVERIFIED_ORIGIN`.
        assertFalse(TwaValidator.isTwaForOrigin(mContext, TEST_SUPPORT_PACKAGE, UNVERIFIED_ORIGIN));
    }

    @Test
    @MediumTest
    public void testIsTwaForOrigin_verifiedDelegateAndDeclaresService_isTwa() {
        InstalledWebappPermissionManager.addDelegateApp(TEST_ORIGIN, TEST_SUPPORT_PACKAGE);
        assertTrue(TwaValidator.isTwaForOrigin(mContext, TEST_SUPPORT_PACKAGE, TEST_ORIGIN));
    }

    @Test
    @MediumTest
    public void testIsTwaInstalledForUrl_empty() {
        assertFalse(TwaValidator.isTwaInstalledForUrl(GURL.emptyGURL()));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void testIsTwaForOrigin_verifiedByDomainVerificationManager_returnsTrue() {
        TwaValidator.setDomainVerificationDelegateForTesting(
                (context, packageName, origin) -> true);

        assertTrue(TwaValidator.isTwaForOrigin(mContext, TEST_SUPPORT_PACKAGE, TEST_ORIGIN));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void testIsTwaForOrigin_domainVerificationNone_fallsBackToDelegate() {
        TwaValidator.setDomainVerificationDelegateForTesting(
                (context, packageName, origin) -> false);

        assertFalse(TwaValidator.isTwaForOrigin(mContext, TEST_SUPPORT_PACKAGE, TEST_ORIGIN));

        InstalledWebappPermissionManager.addDelegateApp(TEST_ORIGIN, TEST_SUPPORT_PACKAGE);
        assertTrue(TwaValidator.isTwaForOrigin(mContext, TEST_SUPPORT_PACKAGE, TEST_ORIGIN));
    }
}
