// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;

import androidx.browser.trusted.Token;
import androidx.browser.trusted.TrustedWebActivityService;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Instrumentation tests for {@link TwaValidator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TwaValidatorTest {
    // Package from test support APK (contains TrustedWebActivityService implementation)
    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";
    private static final String OTHER_TWA_PACKAGE = "com.example.other_twa";
    private static final String NON_TWA_PACKAGE = "com.android.settings";
    private static final String TEST_URL = "https://www.example.com/app";
    private static final Origin TEST_ORIGIN = Origin.create(TEST_URL);
    private static final Origin UNVERIFIED_ORIGIN =
            Origin.create("https://www.unverified-domain.com");

    private static class CountingPermissionStore extends InstalledWebappPermissionStore {
        int mGetAllDelegateAppsCallCount;

        @Override
        public @Nullable Set<Token> getAllDelegateApps(Origin origin) {
            mGetAllDelegateAppsCallCount++;
            return super.getAllDelegateApps(origin);
        }
    }

    private Context mContext;
    private CountingPermissionStore mPermissionStore;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mPermissionStore = new CountingPermissionStore();
        mPermissionStore.clearForTesting();
        WebappRegistry.getInstance().setPermissionStoreForTesting(mPermissionStore);
    }

    @After
    public void tearDown() {
        mPermissionStore.clearForTesting();
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
    public void testIsTwaInstalledForUrl_invalidOrNonHttpUrlThrows() {
        GURL emptyUrl = GURL.emptyGURL();
        GURL invalidUrl = new GURL("not a url");
        GURL aboutBlankUrl = new GURL("about:blank");
        GURL chromeUrl = new GURL("chrome://version");

        assertThrows(
                IllegalArgumentException.class, () -> TwaValidator.isTwaInstalledForUrl(emptyUrl));
        assertThrows(
                IllegalArgumentException.class,
                () -> TwaValidator.isTwaInstalledForUrl(invalidUrl));
        assertThrows(
                IllegalArgumentException.class,
                () -> TwaValidator.isTwaInstalledForUrl(aboutBlankUrl));
        assertThrows(
                IllegalArgumentException.class, () -> TwaValidator.queryFirstTwaPackage(emptyUrl));
        assertThrows(
                IllegalArgumentException.class, () -> TwaValidator.queryFirstTwaPackage(chromeUrl));

        TwaValidator.setTwaInstalledOverrideForTesting(true);
        assertThrows(
                IllegalArgumentException.class,
                () -> TwaValidator.isTwaInstalledForUrl(aboutBlankUrl));
        TwaValidator.setTwaInstalledOverrideForTesting(null);
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

    @Test
    @MediumTest
    public void testHasTwaBeenRunForOrigin() {
        assertFalse(TwaValidator.hasTwaBeenRunForOrigin(TEST_ORIGIN));

        mPermissionStore.setStateForOrigin(
                TEST_ORIGIN,
                TEST_SUPPORT_PACKAGE,
                "Test App",
                ContentSettingsType.NOTIFICATIONS,
                ContentSetting.ALLOW);

        assertTrue(TwaValidator.hasTwaBeenRunForOrigin(TEST_ORIGIN));
    }

    @Test
    @MediumTest
    public void testIsTwaInstalledForUrl_doesNotReadStoreWhenNoTwaServiceCandidate() {
        TwaValidator.setDomainVerificationDelegateForTesting(
                (context, packageName, origin) -> false);

        // When a package does not declare TrustedWebActivityService or no non-browser candidate
        // handles the URL, the permission store is not read.
        assertFalse(TwaValidator.isTwaForOrigin(mContext, NON_TWA_PACKAGE, TEST_ORIGIN));
        GURL unverifiedGurl = new GURL(UNVERIFIED_ORIGIN.toString());
        assertFalse(TwaValidator.isTwaInstalledForUrl(unverifiedGurl));
        assertNull(TwaValidator.queryFirstTwaPackage(unverifiedGurl));
        assertEquals(0, mPermissionStore.mGetAllDelegateAppsCallCount);
    }

    @Test
    @MediumTest
    public void testIsTwaInstalledForUrl_readsStoreLazilyAndMemoizesAcrossCandidates() {
        TwaValidator.setDomainVerificationDelegateForTesting(
                (context, packageName, origin) -> false);
        registerSecondUnverifiedTwaCandidateForTestUrl();

        // For TEST_URL, both OTHER_TWA_PACKAGE and TEST_SUPPORT_PACKAGE handle the URL and
        // declare the TWA service. Before registering TEST_SUPPORT_PACKAGE, the permission store
        // is read once and memoized across both candidates.
        GURL testGurl = new GURL(TEST_URL);
        assertFalse(TwaValidator.isTwaInstalledForUrl(testGurl));
        assertEquals(1, mPermissionStore.mGetAllDelegateAppsCallCount);

        // After registering TEST_SUPPORT_PACKAGE, each query checks OTHER_TWA_PACKAGE first and
        // then matches TEST_SUPPORT_PACKAGE using the memoized delegate apps set.
        InstalledWebappPermissionManager.addDelegateApp(TEST_ORIGIN, TEST_SUPPORT_PACKAGE);
        mPermissionStore.mGetAllDelegateAppsCallCount = 0;

        assertEquals(TEST_SUPPORT_PACKAGE, TwaValidator.queryFirstTwaPackage(testGurl));
        assertEquals(1, mPermissionStore.mGetAllDelegateAppsCallCount);
        assertTrue(TwaValidator.isTwaInstalledForUrl(testGurl));
        assertEquals(2, mPermissionStore.mGetAllDelegateAppsCallCount);

        // The test override short-circuits before reading the permission store.
        TwaValidator.setTwaInstalledOverrideForTesting(false);
        assertFalse(TwaValidator.isTwaInstalledForUrl(testGurl));
        assertEquals(2, mPermissionStore.mGetAllDelegateAppsCallCount);
        TwaValidator.setTwaInstalledOverrideForTesting(null);
    }

    private void registerSecondUnverifiedTwaCandidateForTestUrl() {
        Context baseContext = ContextUtils.getApplicationContext();
        Context testContext =
                new ContextWrapper(baseContext) {
                    @Override
                    public PackageManager getPackageManager() {
                        return new PackageManagerWrapper(baseContext.getPackageManager()) {
                            @Override
                            public List<ResolveInfo> queryIntentActivities(
                                    Intent intent, int flags) {
                                List<ResolveInfo> result =
                                        new ArrayList<>(super.queryIntentActivities(intent, flags));
                                if (TEST_URL.equals(intent.getDataString())) {
                                    ResolveInfo otherCandidate = new ResolveInfo();
                                    otherCandidate.activityInfo = new ActivityInfo();
                                    otherCandidate.activityInfo.packageName = OTHER_TWA_PACKAGE;
                                    result.add(0, otherCandidate);
                                }
                                return result;
                            }

                            @Override
                            public List<ResolveInfo> queryIntentServices(Intent intent, int flags) {
                                if (OTHER_TWA_PACKAGE.equals(intent.getPackage())
                                        && TrustedWebActivityService
                                                .ACTION_TRUSTED_WEB_ACTIVITY_SERVICE
                                                .equals(intent.getAction())) {
                                    return Collections.singletonList(new ResolveInfo());
                                }
                                return super.queryIntentServices(intent, flags);
                            }

                            // PackageManagerWrapper does not forward this method, and
                            // Token.matches() calls it on API 28+.
                            @Override
                            public boolean hasSigningCertificate(
                                    String packageName, byte[] certificate, int type) {
                                return baseContext
                                        .getPackageManager()
                                        .hasSigningCertificate(packageName, certificate, type);
                            }
                        };
                    }
                };
        ContextUtils.initApplicationContextForTests(testContext);
    }
}
