// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link UrlBarData}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarDataTest {
    @Test
    public void forUrlAndText_nonHttpOrHttps_DisplayTextDiffersFromUrl() {
        UrlBarData data =
                UrlBarData.forUrlAndText("data:text/html,blah,blah", "data:text/html,blah", "BLAH");
        Assert.assertEquals("data:text/html,blah,blah", data.url);
        Assert.assertEquals("data:text/html,blah", data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(data.displayText.length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_aboutUri_NoSlashes() {
        final String aboutUrl = "about:blank#verylongurl.totallylegit.notsuspicious.url.com";
        UrlBarData data = UrlBarData.forUrlAndText(aboutUrl, aboutUrl);
        Assert.assertEquals(aboutUrl, data.url);
        Assert.assertEquals(aboutUrl, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(aboutUrl.length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_aboutUri_WithSlashes() {
        final String aboutUrl = "about://blank#verylongurl.totallylegit.notsuspicious.url.com";
        UrlBarData data = UrlBarData.forUrlAndText(aboutUrl, aboutUrl);
        Assert.assertEquals(aboutUrl, data.url);
        Assert.assertEquals(aboutUrl, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(aboutUrl.length(), data.originEndIndex);
    }

    @Test
    public void originSpans() {
        verifyOriginSpan("", null, "");
        verifyOriginSpan("https:", null, "https:");
        verifyOriginSpan("about:blank", null, "about:blank");

        verifyOriginSpan("chrome://flags", null, "chrome://flags");
        verifyOriginSpan("chrome://flags/?egads", null, "chrome://flags/?egads");

        verifyOriginSpan("www.google.com", null, "www.google.com");
        verifyOriginSpan("www.google.com", null, "www.google.com/");
        verifyOriginSpan("www.google.com", "/?q=blah", "www.google.com/?q=blah");

        verifyOriginSpan("https://www.google.com", null, "https://www.google.com");
        verifyOriginSpan("https://www.google.com", null, "https://www.google.com/");
        verifyOriginSpan("https://www.google.com", "/?q=blah", "https://www.google.com/?q=blah");

        // crbug.com/414990
        String testUrl = "https://disneyworld.disney.go.com/special-offers/"
                + "?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|1540wwa14043";
        verifyOriginSpan("https://disneyworld.disney.go.com",
                "/special-offers/?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                        + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|"
                        + "1540wwa14043",
                testUrl);

        // crbug.com/415387
        verifyOriginSpan("ftp://example.com/ftp.html", null, "ftp://example.com/ftp.html");

        // crbug.com/447416
        verifyOriginSpan("file:///dev/blah", null, "file:///dev/blah");
        verifyOriginSpan(
                "javascript:window.alert('hello');", null, "javascript:window.alert('hello');");
        verifyOriginSpan("data:text/html;charset=utf-8,Page%201", null,
                "data:text/html;charset=utf-8,Page%201");
    }

    private void verifyOriginSpan(
            String expectedOrigin, @Nullable String expectedOriginSuffix, String url) {
        UrlBarData urlBarData = UrlBarData.forUrl(url);
        String displayText = urlBarData.displayText.toString();
        Assert.assertEquals(expectedOriginSuffix == null ? expectedOrigin
                                                         : expectedOrigin + expectedOriginSuffix,
                displayText);
        Assert.assertEquals("Origina start index, end index did not generate expected origin",
                expectedOrigin,
                displayText.substring(urlBarData.originStartIndex, urlBarData.originEndIndex));
    }
}
