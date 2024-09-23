// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Unit tests for IntentHandler. These tests require use of the native library. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class IntentHandlerNativeTest {
    private static final String GOOGLE_URL = "https://www.google.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraReferrer() {
        // Check that EXTRA_REFERRER is not accepted with a random URL.
        Intent foreignIntent = new Intent(Intent.ACTION_VIEW);
        foreignIntent.putExtra(Intent.EXTRA_REFERRER, GOOGLE_URL);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(foreignIntent));

        // Check that EXTRA_REFERRER with android-app URL works.
        String appUrl = "android-app://com.application/http/www.application.com";
        Intent appIntent = new Intent(Intent.ACTION_VIEW);
        appIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(appUrl));
        Assert.assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(appIntent));

        // Ditto, with EXTRA_REFERRER_NAME.
        Intent nameIntent = new Intent(Intent.ACTION_VIEW);
        nameIntent.putExtra(Intent.EXTRA_REFERRER_NAME, appUrl);
        Assert.assertEquals(appUrl, IntentHandler.getReferrerUrlIncludingExtraHeaders(nameIntent));

        // Check that EXTRA_REFERRER with an empty host android-app URL doesn't work.
        appUrl = "android-app:///www.application.com";
        appIntent = new Intent(Intent.ACTION_VIEW);
        appIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(appUrl));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(appIntent));

        // Ditto, with EXTRA_REFERRER_NAME.
        nameIntent = new Intent(Intent.ACTION_VIEW);
        nameIntent.putExtra(Intent.EXTRA_REFERRER_NAME, appUrl);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(nameIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersInclReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Accept", "application/xhtml+xml");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals(
                "Accept: application/xhtml+xml",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersInclRefererMultiple() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Accept", "application/xhtml+xml");
        bundle.putString("Content-Language", "de-DE, en-CA");
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals(
                "Content-Language: de-DE, en-CA\nAccept: application/xhtml+xml",
                IntentHandler.getExtraHeadersFromIntent(headersIntent));
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersOnlyReferer() {
        // Check that invalid header specified in EXTRA_HEADERS isn't used.
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersAndExtraReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", GOOGLE_URL);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        headersIntent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(validReferer));
        Assert.assertEquals(
                validReferer, IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testReferrerUrl_extraHeadersValidReferrer() {
        String validReferer = "android-app://package/http/url";
        Bundle bundle = new Bundle();
        bundle.putString("Referer", validReferer);
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertEquals(
                validReferer, IntentHandler.getReferrerUrlIncludingExtraHeaders(headersIntent));
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    public void testExtraHeadersVerifiedOrigin() throws Exception {
        // Check that non-allowlisted headers from extras are passed
        // when origin is verified.
        Context context = ApplicationProvider.getApplicationContext();
        Intent headersIntent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context, "https://www.google.com/");

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        headersIntent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(headersIntent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                "app1",
                                Origin.create(headersIntent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        String extraHeaders = IntentHandler.getExtraHeadersFromIntent(headersIntent);
        assertTrue(extraHeaders.contains("bearer-token: Some token"));
        assertTrue(extraHeaders.contains("redirect-url: https://www.google.com"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeOriginVerifier.clearCachedVerificationsForTesting());
    }

    @Test
    @SmallTest
    public void testExtraHeadersNonVerifiedOrigin() throws Exception {
        // Check that non-allowlisted headers from extras are passed
        // when origin is verified.
        Context context = ApplicationProvider.getApplicationContext();
        Intent headersIntent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context, "https://www.google.com/");

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        headersIntent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(headersIntent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                "app2",
                                Origin.create(headersIntent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        String extraHeaders = IntentHandler.getExtraHeadersFromIntent(headersIntent);
        assertNull(extraHeaders);
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeOriginVerifier.clearCachedVerificationsForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testRemoveChromeCustomHeaderFromExtraIntentHeaders() {
        Bundle bundle = new Bundle();
        bundle.putString("X-Chrome-intent-type", "X-custom-value");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testStripNonCorsSafelistedCustomHeader() {
        Bundle bundle = new Bundle();
        bundle.putString("X-Some-Header", "1");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testIgnoreHeaderNewLineInValue() {
        Bundle bundle = new Bundle();
        bundle.putString("sec-ch-ua-full", "\nCookie: secret=cookie");
        Intent headersIntent = new Intent(Intent.ACTION_VIEW);
        headersIntent.putExtra(Browser.EXTRA_HEADERS, bundle);
        Assert.assertNull(IntentHandler.getExtraHeadersFromIntent(headersIntent));
    }
}
