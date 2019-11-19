// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for non-native part of {@link UrlUtilities}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlUtilitiesUnitTest {
    @Test
    public void testIsHttpOrHttps() {
        Assert.assertTrue(
                UrlUtilities.isHttpOrHttps("https://user:pass@awesome.com:9000/bad-scheme/#fake"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http://awesome.example.com/"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:example.com"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:go"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("https:example.com://looks-invalid-but-not"));
        // The [] in path would trigger java.net.URI to throw URISyntaxException, but works fine in
        // java.net.URL.
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http://foo.bar/has[square].html"));

        Assert.assertFalse(UrlUtilities.isHttpOrHttps("example.com"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("about:awesome"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("data:data"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("file://hostname/path/to/file"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("inline:skates.co.uk"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("javascript:alert(1)"));

        Assert.assertFalse(UrlUtilities.isHttpOrHttps("super:awesome"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/"));
        Assert.assertFalse(
                UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps(
                "google-search://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("chrome://http://version"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps(""));
    }

    @Test
    public void testValidateIntentUrl() {
        // Valid action, hostname, and (empty) path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://10010#Intent;scheme=tel;action=com.google.android.apps."
                + "authenticator.AUTHENTICATE;end"));
        // Valid package, scheme, hostname, and path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://scan/#Intent;package=com.google.zxing.client.android;"
                + "scheme=zxing;end;"));
        // Valid package, scheme, component, hostname, and path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=yow;component=com.example.PUMPKIN;end;"));
        // Valid package, scheme, action, hostname, and path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=eeek;action=frighten_children;end;"));
        // Valid package, component, String extra, hostname, and path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;S.goat=leg;end"));

        // Valid package, component, int extra (with URL-encoded key), String
        // extra, hostname, and path.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;i.pumpkinCount%3D=42;"
                + "S.goat=leg;end"));

        // Android's Intent.toUri does not generate URLs like this, but
        // Google Authenticator does, and we must handle them.
        Assert.assertTrue(UrlUtilities.validateIntentUrl(
                "intent:#Intent;action=com.google.android.apps.chrome."
                + "TEST_AUTHENTICATOR;category=android.intent.category."
                + "BROWSABLE;S.inputData=cancelled;end"));

        // null does not have a valid intent scheme.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(null));
        // The empty string does not have a valid intent scheme.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(""));
        // A whitespace string does not have a valid intent scheme.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(" "));
        // Junk after end.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(
                "intent://10010#Intent;scheme=tel;action=com.google.android.apps."
                + "authenticator.AUTHENTICATE;end','*');"
                + "alert(document.cookie);//"));
        // component appears twice.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=yow;component=com.example.PUMPKIN;"
                + "component=com.example.AVOCADO;end;"));
        // scheme contains illegal character.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "scheme=hello+goodbye;component=com.example.PUMPKIN;end;"));
        // category contains illegal character.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(
                "intent://wump-hey.example.com/#Intent;package=com.example.wump;"
                + "category=42%_by_volume;end"));
        // Incorrectly URL-encoded.
        Assert.assertFalse(UrlUtilities.validateIntentUrl(
                "intent://testing/#Intent;package=cybergoat.noodle.crumpet;"
                + "component=wump.noodle/Crumpet;i.pumpkinCount%%3D=42;"
                + "S.goat=&leg;end"));
    }

    @Test
    public void testStripPath() {
        Assert.assertEquals("https://example.com:9000",
                UrlUtilities.stripPath("https://user:pass@example.com:9000/path/#extra"));
        Assert.assertEquals("http://awesome.example.com",
                UrlUtilities.stripPath("http://awesome.example.com/?query"));
        Assert.assertEquals("http://localhost", UrlUtilities.stripPath("http://localhost/"));
        Assert.assertEquals("http://", UrlUtilities.stripPath("http:"));
    }

    @Test
    public void testStripScheme() {
        // Only scheme gets stripped.
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("https://cs.chromium.org"));
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("http://cs.chromium.org"));
        // If there is no scheme, nothing changes.
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("cs.chromium.org"));
        // Path is not touched/changed.
        String urlWithPath = "code.google.com/p/chromium/codesearch#search"
                + "/&q=testStripScheme&sq=package:chromium&type=cs";
        Assert.assertEquals(urlWithPath, UrlUtilities.stripScheme("https://" + urlWithPath));
        // Beginning and ending spaces get trimmed.
        Assert.assertEquals(
                "cs.chromium.org", UrlUtilities.stripScheme("  https://cs.chromium.org  "));
    }
}
