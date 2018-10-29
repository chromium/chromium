// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Collection;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/**
 * Tests for WebsitePermissionsFetcher.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebsitePermissionsFetcherTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String[] PERMISSION_URLS = {
            "http://www.google.com/", "http://www.youtube.com/", "http://www.facebook.com/",
            "http://www.baidu.com/", "http://www.wikipedia.org/", "http://www.yahoo.com/",
            "http://www.google.co.in/", "http://www.reddit.com/", "http://www.qq.com/",
            "http://www.taobao.com/", "http://www.google.co.jp/", "http://www.amazon.com/",
            "http://www.twitter.com/", "http://www.live.com/", "http://www.instagram.com/",
            "http://www.weibo.com/", "http://www.google.de/", "http://www.google.co.uk/",
            "http://www.google.com.br/", "http://www.google.fr/", "http://www.google.ru/",
            "http://www.linkedin.com/", "http://www.google.com.hk/", "http://www.yandex.ru/",
            "http://www.google.it/", "http://www.netflix.com/", "http://www.yahoo.co.jp/",
            "http://www.google.es/", "http://www.t.co/", "http://www.google.com.mx/",
            "http://www.google.ca/", "http://www.ebay.com/", "http://www.alipay.com/",
            "http://www.bing.com/", "http://www.imgur.com/", "http://www.twitch.tv/",
            "http://www.msn.com/", "http://www.apple.com/", "http://www.aliexpress.com/",
            "http://www.microsoft.com/", "http://www.wordpress.com/", "http://www.office.com/",
            "http://www.mail.ru/", "http://www.tumblr.com/", "http://www.stackoverflow.com/",
            "http://www.microsoftonline.com/", "http://www.imdb.com/", "http://www.github.com/",
            "http://www.blogspot.com/", "http://www.amazon.co.jp/", "http://www.google.com.au/",
            "http://www.google.com.tw/", "http://www.google.com.tr/", "http://www.paypal.com/",
            "http://www.google.pl/", "http://www.wikia.com/", "http://www.pinterest.com/",
            "http://www.whatsapp.com/", "http://www.google.co.id/", "http://www.espn.com/",
            "http://www.adobe.com/", "http://www.google.com.ar/",
            "http://www.googleusercontent.com/", "http://www.amazon.in/", "http://www.dropbox.com/",
            "http://www.amazon.de/", "http://www.google.com.ua/", "http://www.so.com/",
            "http://www.google.com.pk/", "http://www.cnn.com/", "http://www.amazon.co.uk/",
            "http://www.bbc.co.uk/", "http://www.google.com.sa/", "http://www.craigslist.org/",
            "http://www.bbc.com/", "http://www.google.co.th/", "http://www.google.com.eg/",
            "http://www.google.nl/", "http://www.amazonaws.com/", "http://www.soundcloud.com/",
            "http://www.ask.com/", "http://www.google.co.za/", "http://www.booking.com/",
            "http://www.nytimes.com/", "http://www.google.co.ve/", "http://www.google.co.kr/",
            "http://www.quora.com/", "http://www.mozilla.org/", "http://www.dailymotion.com/",
            "http://www.stackexchange.com/", "http://www.salesforce.com/", "http://www.detik.com/",
            "http://www.blogger.com/", "http://www.ebay.de/", "http://www.vimeo.com/",
            "http://www.theguardian.com/", "http://www.tribunnews.com/",
            "http://www.google.com.sg/", "http://www.google.gr/", "http://www.flipkart.com/",
            "http://www.spotify.com/", "http://www.slideshare.net/", "http://www.chase.com/",
            "http://www.google.com.ph/", "http://www.ebay.co.uk/", "http://www.google.se/",
            "http://www.cnet.com/", "http://www.google.be/", "http://www.nih.gov/",
            "http://www.google.cn/", "http://www.steamcommunity.com/", "http://www.mediafire.com/",
            "http://www.xinhuanet.com/", "http://www.google.az/", "http://www.vice.com/",
            "http://www.alibaba.com/", "http://www.dailymail.co.uk/", "http://www.google.com.co/",
            "http://www.buzzfeed.com/", "http://www.doubleclick.net/", "http://www.google.com.ng/",
            "http://www.google.co.ao/", "http://www.google.at/", "http://www.uol.com.br/",
            "http://www.redd.it/", "http://www.deviantart.com/", "http://www.washingtonpost.com/",
            "http://www.twimg.com/", "http://www.w3schools.com/", "http://www.godaddy.com/",
            "http://www.wikihow.com/", "http://www.etsy.com/", "http://www.slack.com/",
            "http://www.google.cz/", "http://www.google.ch/", "http://www.amazon.it/",
            "http://www.steampowered.com/", "http://www.google.com.vn/", "http://www.walmart.com/",
            "http://www.messenger.com/", "http://www.discordapp.com/", "http://www.google.cl/",
            "http://www.amazon.fr/", "http://www.yelp.com/", "http://www.google.no/",
            "http://www.google.pt/", "http://www.google.ae/", "http://www.weather.com/",
            "http://www.mercadolivre.com.br/", "http://www.google.ro/",
            "http://www.bankofamerica.com/", "http://www.blogspot.co.id/", "http://www.trello.com/",
            "http://www.gfycat.com/", "http://www.ikea.com/", "http://www.scribd.com/",
            "http://www.china.com.cn/", "http://www.forbes.com/", "http://www.wellsfargo.com/",
            "http://www.indiatimes.com/", "http://www.cnblogs.com/", "http://www.gamepedia.com/",
            "http://www.outbrain.com/", "http://www.9gag.com/", "http://www.google.ie/",
            "http://www.gearbest.com/", "http://www.files.wordpress.com/",
            "http://www.huffingtonpost.com/", "http://www.speedtest.net/", "http://www.google.dk/",
            "http://www.google.fi/", "http://www.wikimedia.org/", "http://www.thesaurus.com/",
            "http://www.livejournal.com/", "http://www.nfl.com/", "http://www.zillow.com/",
            "http://www.foxnews.com/", "http://www.researchgate.net/", "http://www.amazon.cn/",
            "http://www.google.hu/", "http://www.google.co.il/", "http://www.amazon.es/",
            "http://www.wordreference.com/", "http://www.blackboard.com/", "http://www.google.dz/",
            "http://www.tripadvisor.com/", "http://www.shutterstock.com/",
            "http://www.cloudfront.net/", "http://www.aol.com/", "http://www.weebly.com/",
            "http://www.battle.net/", "http://www.archive.org/",
    };

    private static class WebsitePermissionsWaiter
            extends CallbackHelper implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            notifyCalled();
        }
    }

    @Test
    @SmallTest
    public void testNullsCanBeHandled() throws Exception {
        // This is a smoke test to ensure that nulls do not cause crashes.
        WebsitePermissionsFetcher.OriginAndEmbedder nullBoth =
                new WebsitePermissionsFetcher.OriginAndEmbedder(null, null);

        WebsitePermissionsFetcher.OriginAndEmbedder nullOrigin =
                new WebsitePermissionsFetcher.OriginAndEmbedder(
                        WebsiteAddress.create("https://www.google.com"), null);

        WebsitePermissionsFetcher.OriginAndEmbedder nullEmbedder =
                new WebsitePermissionsFetcher.OriginAndEmbedder(
                        null, WebsiteAddress.create("https://www.google.com"));

        HashMap<WebsitePermissionsFetcher.OriginAndEmbedder, String> map = new HashMap<>();

        map.put(nullBoth, "nullBoth");
        map.put(nullOrigin, "nullOrigin");
        map.put(nullEmbedder, "nullEmbedder");

        Assert.assertTrue(map.containsKey(nullBoth));
        Assert.assertTrue(map.containsKey(nullOrigin));
        Assert.assertTrue(map.containsKey(nullEmbedder));

        Assert.assertEquals("nullBoth", map.get(nullBoth));
        Assert.assertEquals("nullOrigin", map.get(nullOrigin));
        Assert.assertEquals("nullEmbedder", map.get(nullEmbedder));
    }

    @Test
    @SmallTest
    public void testFetcherDoesNotTimeOutWithManyUrls() throws Exception {
        final WebsitePermissionsWaiter waiter = new WebsitePermissionsWaiter();
        // Set lots of permissions values.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (String url : PERMISSION_URLS) {
                    WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                            url, url, ContentSetting.BLOCK.toInt(), false);
                    WebsitePreferenceBridge.nativeSetMidiSettingForOrigin(
                            url, url, ContentSetting.ALLOW.toInt(), false);
                    WebsitePreferenceBridge.nativeSetProtectedMediaIdentifierSettingForOrigin(
                            url, url, ContentSetting.BLOCK.toInt(), false);
                    WebsitePreferenceBridge.nativeSetNotificationSettingForOrigin(
                            url, ContentSetting.ALLOW.toInt(), false);
                    WebsitePreferenceBridge.nativeSetMicrophoneSettingForOrigin(
                            url, ContentSetting.ALLOW.toInt(), false);
                    WebsitePreferenceBridge.nativeSetCameraSettingForOrigin(
                            url, ContentSetting.BLOCK.toInt(), false);
                }

                // This should not time out. See crbug.com/732907.
                WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
                fetcher.fetchAllPreferences(waiter);
            }
        });
        waiter.waitForCallback(0, 1, scaleTimeout(1000), TimeUnit.MILLISECONDS);
    }
}
