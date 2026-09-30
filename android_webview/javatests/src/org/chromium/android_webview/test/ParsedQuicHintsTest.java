// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.ParsedQuicHints;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** {@link ParsedQuicHints} tests. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS)
@Batch(Batch.PER_CLASS)
public class ParsedQuicHintsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule =
            new AwActivityTestRule() {
                @Override
                public boolean needsBrowserProcessStarted() {
                    return false;
                }
            };

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testParseQuicHintsSuccess() {
        ParsedQuicHints parsed =
                ParsedQuicHints.parse(
                        Set.of(
                                "https://*.google.com",
                                "HTTPS://*.EXAMPLE.COM",
                                "https://*.example.com:443",
                                "https://*.example.com:8443",
                                "https://*.example.com/api",
                                "https://*.query-only.test?query=1",
                                "https://*.fragment-only.test#hash",
                                "https://*.port-and-query.test:8443?query=1#hash",
                                "https://*.trailing-dot.test.",
                                "https://*.sub.test",
                                "https://*.sub.test:9443",
                                "https://foo.test:443",
                                "https://example.com",
                                "https://example.com/index.html",
                                "https://foo.test:443/some/path?query=1#hash",
                                "https://[::1]",
                                "https://[::1]:8443"));

        Assert.assertFalse(parsed.tryQuicByDefault);
        Assert.assertEquals(9, parsed.wildcardSuffixes.length);
        Assert.assertEquals(
                Set.of(
                        ".google.com",
                        ".example.com",
                        ".example.com:8443",
                        ".query-only.test",
                        ".fragment-only.test",
                        ".port-and-query.test:8443",
                        ".trailing-dot.test.",
                        ".sub.test",
                        ".sub.test:9443"),
                new HashSet<>(Arrays.asList(parsed.wildcardSuffixes)));
        Assert.assertEquals(6, parsed.exactOrigins.length);
        Assert.assertEquals(
                Set.of(
                        new GURL("https://foo.test:443"),
                        new GURL("https://example.com"),
                        new GURL("https://example.com/index.html"),
                        new GURL("https://foo.test:443/some/path?query=1#hash"),
                        new GURL("https://[::1]"),
                        new GURL("https://[::1]:8443")),
                new HashSet<>(Arrays.asList(parsed.exactOrigins)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testParseQuicHintsCatchAll() {
        ParsedQuicHints starOnly = ParsedQuicHints.parse(Set.of("*"));
        Assert.assertTrue(starOnly.tryQuicByDefault);
        Assert.assertEquals(0, starOnly.exactOrigins.length);
        Assert.assertEquals(0, starOnly.wildcardSuffixes.length);

        ParsedQuicHints httpsStar =
                ParsedQuicHints.parse(
                        Set.of("https://*", "https://*.example.com", "https://example.com"));
        Assert.assertTrue(httpsStar.tryQuicByDefault);
        Assert.assertEquals(0, httpsStar.exactOrigins.length);
        Assert.assertEquals(0, httpsStar.wildcardSuffixes.length);
    }

    private static void assertInvalidQuicHints(Set<String> origins) {
        Assert.assertThrows(IllegalArgumentException.class, () -> ParsedQuicHints.parse(origins));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testParseQuicHintsFailure() {
        assertInvalidQuicHints(Set.of("not a valid origin"));
        assertInvalidQuicHints(Set.of("*.example.com"));
        assertInvalidQuicHints(Set.of(".example.com"));
        assertInvalidQuicHints(Set.of("example.com"));
        assertInvalidQuicHints(Set.of("https://*.example.com:99999"));
        assertInvalidQuicHints(Set.of("https://*.example.com:abc"));
        assertInvalidQuicHints(Set.of("https://*.example.com:0"));
        assertInvalidQuicHints(Set.of("https://example.com:0"));
        assertInvalidQuicHints(Set.of(""));
        assertInvalidQuicHints(Set.of("   "));
        assertInvalidQuicHints(Set.of("http://example.com"));
        assertInvalidQuicHints(Set.of("http://*.example.com"));
        assertInvalidQuicHints(Set.of("https://*.invalid..domain"));
        assertInvalidQuicHints(Set.of("https://*..example.com"));
        assertInvalidQuicHints(Set.of("https://*.example.com.."));
        assertInvalidQuicHints(Set.of("https://*."));
        assertInvalidQuicHints(Set.of("https://*./path"));
        assertInvalidQuicHints(Set.of("https://*./example.com"));
        assertInvalidQuicHints(Set.of("https://*.\\example.com"));
        assertInvalidQuicHints(Set.of("https://*.?query=1"));
        assertInvalidQuicHints(Set.of("https://*.#hash"));
        assertInvalidQuicHints(Set.of("https://*.user@example.com"));
        assertInvalidQuicHints(Set.of("https://*.user:pass@example.com:8443"));
        assertInvalidQuicHints(Set.of("https://*.127.0.0.1"));
        assertInvalidQuicHints(Set.of("https://*.[::1]"));
        assertInvalidQuicHints(Set.of("https://*.[::1]:443"));
        assertInvalidQuicHints(Set.of("*", "not a valid origin"));
        assertInvalidQuicHints(Collections.singleton(null));
        assertInvalidQuicHints(null);
    }
}
