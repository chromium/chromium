// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.webapk.lib.common.WebApkMetaDataKeys.SCOPE;
import static org.chromium.webapk.lib.common.WebApkMetaDataKeys.START_URL;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.testing.local.TestDir;

import java.net.URISyntaxException;

/** Unit tests for {@link org.chromium.webapk.lib.client.WebApkValidator}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkValidatorTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.foo";
    private static final String INVALID_WEBAPK_PACKAGE_NAME = "invalid.org.chromium.webapk.foo";
    private static final String URL_OF_WEBAPK = "https://www.foo.com";
    private static final String URL_WITHOUT_WEBAPK = "https://www.other.com";
    private static final String TEST_DATA_DIR = "webapks/";
    private static final String TEST_STARTURL = "https://non-empty.com/starturl";
    private static final String MAPSLITE_PACKAGE_NAME = "com.google.android.apps.mapslite";
    private static final String MAPSLITE_EXAMPLE_STARTURL = "https://www.google.com/maps";

    private static final byte[] EXPECTED_SIGNATURE = new byte[] {48, -126, 3, -121, 48, -126, 2,
            111, -96, 3, 2, 1, 2, 2, 4, 20, -104, -66, -126, 48, 13, 6, 9, 42, -122, 72, -122, -9,
            13, 1, 1, 11, 5, 0, 48, 116, 49, 11, 48, 9, 6, 3, 85, 4, 6, 19, 2, 67, 65, 49, 16, 48,
            14, 6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97, 114, 105, 111, 49, 17, 48, 15, 6, 3, 85, 4,
            7, 19, 8, 87, 97, 116, 101, 114, 108, 111, 111, 49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8,
            67, 104, 114, 111, 109, 105, 117, 109, 49, 17, 48};

    private static final byte[] SIGNATURE_1 = new byte[] {13, 52, 51, 48, 51, 48, 51, 49, 53, 49,
            54, 52, 52, 90, 48, 116, 49, 11, 48, 9, 6, 3, 85, 4, 6, 19, 2, 67, 65, 49, 16, 48, 14,
            6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97, 114};

    private static final byte[] SIGNATURE_2 = new byte[] {49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8,
            67, 104, 114, 111, 109, 105, 117, 109, 49, 17, 48, 15, 6, 3, 85, 4, 11, 19, 8, 67, 104,
            114, 111, 109, 105, 117, 109, 49, 26, 48, 24};

    // This is the public key used for the test files (chrome/test/data/webapks/public.der)
    private static final byte[] PUBLIC_KEY = new byte[] {48, 89, 48, 19, 6, 7, 42, -122, 72, -50,
            61, 2, 1, 6, 8, 42, -122, 72, -50, 61, 3, 1, 7, 3, 66, 0, 4, -67, 14, 37, -20, 103, 121,
            124, -60, -21, 83, -114, -120, -87, -38, 26, 78, 82, 55, 44, -23, -2, 104, 115, 82, -55,
            -104, 105, -19, -48, 89, -65, 12, -31, 16, -35, 4, -121, -70, -89, 23, 56, 115, 112, 78,
            -65, 114, -103, 120, -88, -112, -102, -61, 72, -16, 74, 53, 50, 49, -56, -48, -90, 5,
            -116, 78};

    private ShadowPackageManager mPackageManager;

    @Before
    public void setUp() {
        mPackageManager = Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        WebApkValidator.init(EXPECTED_SIGNATURE, PUBLIC_KEY);
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns a WebAPK's package name if
     * the WebAPK can handle the given URL and the WebAPK is valid.
     */
    @Test
    public void testQueryWebApkPackageReturnsPackageIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertEquals(WEBAPK_PACKAGE_NAME,
                    WebApkValidator.queryFirstWebApkPackage(
                            RuntimeEnvironment.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns null for a non-browsable
     * Intent.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullForNonBrowsableIntent() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertNull(WebApkValidator.queryFirstWebApkPackage(
                    RuntimeEnvironment.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.queryFirstWebApkPackage()} returns null if no WebAPK handles the
     * given URL.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullWhenNoWebApkHandlesTheURL() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertNull(WebApkValidator.queryFirstWebApkPackage(
                    RuntimeEnvironment.application, URL_WITHOUT_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns true if the
     * WebAPK can handle the given URL and the WebAPK is valid.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsTrueIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertTrue(WebApkValidator.canWebApkHandleUrl(
                    RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false if the given APK package
     * name is not signed with the WebAPK signature.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseIfWebApkIsNotValid() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(SIGNATURE_1), TEST_STARTURL));

            assertFalse(WebApkValidator.canWebApkHandleUrl(
                    RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false for a non-browsable WebAPK.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseForNonBrowsableIntent() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertFalse(WebApkValidator.canWebApkHandleUrl(
                    RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.canWebApkHandleUrl()} returns false if the specific WebAPK does
     * not handle the given URL.
     */
    @Test
    public void testCanWebApkHandleUrlReturnsFalseWhenNoWebApkHandlesTheURL() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(WEBAPK_PACKAGE_NAME);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

            assertFalse(WebApkValidator.canWebApkHandleUrl(
                    RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, URL_WITHOUT_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns true if a package name corresponds to a
     * WebAPK and the WebAPK is valid.
     */
    @Test
    public void testIsValidWebApkReturnsTrueForValidWebApk() {
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

        assertTrue(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if the package
     * name is not valid for WebApks (and isn't comment-signed).
     */
    @Test
    public void testIsValidWebApkFalseForInvalidPackageName() {
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                INVALID_WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE), TEST_STARTURL));

        assertFalse(WebApkValidator.isValidWebApk(
                RuntimeEnvironment.application, INVALID_WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns true if the package
     * name is maps lite and the start url matches the correct prefix.
     */
    @Test
    public void testIsValidWebApkForMapsLite() {
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                MAPSLITE_PACKAGE_NAME, new Signature(SIGNATURE_1), MAPSLITE_EXAMPLE_STARTURL));
        mPackageManager.addPackage(
                newPackageInfoWithBrowserSignature(MAPSLITE_PACKAGE_NAME + ".other",
                        new Signature(SIGNATURE_1), MAPSLITE_EXAMPLE_STARTURL));

        assertTrue(WebApkValidator.isValidWebApk(
                RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME));
        assertFalse(WebApkValidator.isValidWebApk(
                RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME + ".other"));
        assertFalse(WebApkValidator.isValidWebApk(
                RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME + ".notfound"));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false when the
     * startUrl is not correct.
     */
    @Test
    public void testIsNotValidWebApkForMapsLiteBadStartUrl() {
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                MAPSLITE_PACKAGE_NAME, new Signature(SIGNATURE_1), TEST_STARTURL));
        assertFalse(WebApkValidator.isValidWebApk(
                RuntimeEnvironment.application, MAPSLITE_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if a WebAPK has more than 2
     * signatures, even if the second one matches the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForMoreThanTwoSignatures() {
        Signature[] signatures = new Signature[] {new Signature(SIGNATURE_1),
                new Signature(EXPECTED_SIGNATURE), new Signature(SIGNATURE_2)};
        mPackageManager.addPackage(
                newPackageInfo(WEBAPK_PACKAGE_NAME, signatures, null, TEST_STARTURL));

        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.isValidWebApk} returns false if a WebAPK has multiple
     * signatures but none of the signatures match the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForWebApkWithMultipleSignaturesWithoutAnyMatched() {
        Signature signatures[] =
                new Signature[] {new Signature(SIGNATURE_1), new Signature(SIGNATURE_2)};
        mPackageManager.addPackage(
                newPackageInfo(WEBAPK_PACKAGE_NAME, signatures, null, TEST_STARTURL));

        assertFalse(
                WebApkValidator.isValidWebApk(RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator#isValidWebApk()} for valid comment signed webapks.
     */
    @Test
    public void testIsValidWebApkCommentSigned() {
        String[] filenames = {"example.apk", "java-example.apk", "v2-signed-ok.apk"};
        String packageName = "com.webapk.a9c419502bb98fcb7";
        Signature[] signature = new Signature[] {new Signature(SIGNATURE_1)};

        for (String filename : filenames) {
            mPackageManager.removePackage(packageName);
            mPackageManager.addPackage(
                    newPackageInfo(packageName, signature, testFilePath(filename), TEST_STARTURL));
            assertTrue(filename + " did not verify",
                    WebApkValidator.isValidWebApk(RuntimeEnvironment.application, packageName));
        }
    }

    /**
     * Tests {@link WebApkValidator#isValidWebApk()} for failing comment signed webapks.
     * These  WebAPKs were modified to fail in specific ways.
     */
    @Test
    public void testIsValidWebApkCommentSignedFailures() {
        String[] filenames = {
                "bad-sig.apk", "bad-utf8-fname.apk", "empty.apk", "extra-field-too-large.apk",
                "extra-len-too-large.apk", "fcomment-too-large.apk", "no-cd.apk", "no-comment.apk",
                "no-eocd.apk", "no-lfh.apk", "not-an.apk", "too-many-metainf.apk", "truncated.apk",
                "zeros.apk", "zeros-at-end.apk", "block-before-first.apk", "block-at-end.apk",
                "block-before-eocd.apk", "block-before-cd.apk", "block-middle.apk",
                "v2-signed-too-large.apk",
        };
        String packageName = "com.webapk.a9c419502bb98fcb7";
        Signature[] signature = new Signature[] {new Signature(SIGNATURE_1)};

        for (String filename : filenames) {
            mPackageManager.removePackage(packageName);
            mPackageManager.addPackage(
                    newPackageInfo(packageName, signature, testFilePath(filename), TEST_STARTURL));
            assertFalse(filename,
                    WebApkValidator.isValidWebApk(RuntimeEnvironment.application, packageName));
        }
    }

    // Get the full test file path.
    private static String testFilePath(String fileName) {
        return TestDir.getTestFilePath(TEST_DATA_DIR + fileName);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private static PackageInfo newPackageInfo(
            String packageName, Signature[] signatures, String sourceDir, String startUrl) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.signatures = signatures;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.metaData = new Bundle();
        packageInfo.applicationInfo.metaData.putString(START_URL, startUrl + "?morestuff");
        packageInfo.applicationInfo.metaData.putString(SCOPE, startUrl);
        packageInfo.applicationInfo.sourceDir = sourceDir;
        return packageInfo;
    }

    // The browser signature is expected to always be the second signature - the first (and any
    // additional ones after the second) are ignored.
    private static PackageInfo newPackageInfoWithBrowserSignature(
            String packageName, Signature signature, String startUrl) {
        return newPackageInfo(
                packageName, new Signature[] {new Signature(""), signature}, null, startUrl);
    }
}
