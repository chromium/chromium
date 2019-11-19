
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
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
    public void testNullsCanBeHandled() {
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (String url : PERMISSION_URLS) {
                WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                        url, url, ContentSettingValues.BLOCK, false);
                WebsitePreferenceBridgeJni.get().setMidiSettingForOrigin(
                        url, url, ContentSettingValues.ALLOW, false);
                WebsitePreferenceBridgeJni.get().setProtectedMediaIdentifierSettingForOrigin(
                        url, url, ContentSettingValues.BLOCK, false);
                WebsitePreferenceBridgeJni.get().setNotificationSettingForOrigin(
                        url, ContentSettingValues.ALLOW, false);
                WebsitePreferenceBridgeJni.get().setMicrophoneSettingForOrigin(
                        url, ContentSettingValues.ALLOW, false);
                WebsitePreferenceBridgeJni.get().setCameraSettingForOrigin(
                        url, ContentSettingValues.BLOCK, false);
            }

            // This should not time out. See crbug.com/732907.
            WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
            fetcher.fetchAllPreferences(waiter);
        });
        waiter.waitForCallback(0, 1, 1000L, TimeUnit.MILLISECONDS);
    }

    class FakeWebsitePreferenceBridge extends WebsitePreferenceBridge {
        public List<PermissionInfo> mPermissionInfos;
        public List<ContentSettingException> mContentSettingExceptions;
        public List<ChosenObjectInfo> mChosenObjectInfos;
        public HashMap<String, LocalStorageInfo> mLocalStorageInfoMap;
        public HashMap<String, LocalStorageInfo> mImportantLocalStorageInfoMap;
        public ArrayList<StorageInfo> mStorageInfos;

        FakeWebsitePreferenceBridge() {
            mPermissionInfos = new ArrayList<>();
            mContentSettingExceptions = new ArrayList<>();
            mChosenObjectInfos = new ArrayList<>();
            mLocalStorageInfoMap = new HashMap<>();
            mImportantLocalStorageInfoMap = new HashMap<>();
            mStorageInfos = new ArrayList<>();
        }

        @Override
        public List<PermissionInfo> getPermissionInfo(@PermissionInfo.Type int type) {
            List<PermissionInfo> result = new ArrayList<>();
            for (PermissionInfo info : mPermissionInfos) {
                if (info.getType() == type) {
                    result.add(info);
                }
            }
            return result;
        }

        @Override
        public List<ContentSettingException> getContentSettingsExceptions(
                @ContentSettingsType int contentSettingsType) {
            List<ContentSettingException> result = new ArrayList<>();
            for (ContentSettingException exception : mContentSettingExceptions) {
                if (exception.getContentSettingType() == contentSettingsType) {
                    result.add(exception);
                }
            }
            return result;
        }

        @Override
        public void fetchLocalStorageInfo(Callback<HashMap> callback, boolean fetchImportant) {
            if (fetchImportant) {
                callback.onResult(mImportantLocalStorageInfoMap);
                return;
            }
            callback.onResult(mLocalStorageInfoMap);
        }

        @Override
        public void fetchStorageInfo(Callback<ArrayList> callback) {
            callback.onResult(mStorageInfos);
        }

        @Override
        public List<ChosenObjectInfo> getChosenObjectInfo(int contentSettingsType) {
            List<ChosenObjectInfo> result = new ArrayList<>();
            for (ChosenObjectInfo info : mChosenObjectInfos) {
                if (info.getContentSettingsType() == contentSettingsType) {
                    result.add(info);
                }
            }
            return result;
        }

        public void addPermissionInfo(PermissionInfo info) {
            mPermissionInfos.add(info);
        }

        public void addContentSettingException(ContentSettingException exception) {
            mContentSettingExceptions.add(exception);
        }

        public void addLocalStorageInfoMapEntry(LocalStorageInfo info) {
            if (info.isDomainImportant()) {
                mImportantLocalStorageInfoMap.put(info.getOrigin(), info);
                return;
            }
            mLocalStorageInfoMap.put(info.getOrigin(), info);
        }

        public void addStorageInfo(StorageInfo info) {
            mStorageInfos.add(info);
        }

        public void addChosenObjectInfo(ChosenObjectInfo info) {
            mChosenObjectInfos.add(info);
        }
    }

    @Test
    @SmallTest
    public void testFetchAllPreferencesForSingleOrigin() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        // Add permission info types.
        Assert.assertEquals(9, PermissionInfo.Type.NUM_ENTRIES);
        String googleOrigin = "https://google.com";
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.GEOLOCATION, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(PermissionInfo.Type.MIDI, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(PermissionInfo.Type.NFC, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.NOTIFICATION, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(PermissionInfo.Type.CAMERA, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.MICROPHONE, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.CLIPBOARD, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(PermissionInfo.Type.SENSORS, googleOrigin, googleOrigin, false));

        // Add content setting exception types.
        String preferenceSource = "preference";
        // If the ContentSettingsType.NUM_TYPES value changes *and* a new value has been exposed on
        // Android, then please update this code block to include a test for your new type.
        // Otherwise, just update count in the assert.
        Assert.assertEquals(55, ContentSettingsType.NUM_TYPES);
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.COOKIES, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.POPUPS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.ADS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.JAVASCRIPT, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.SOUND, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.BACKGROUND_SYNC, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.AUTOMATIC_DOWNLOADS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.AUTOPLAY, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource));

        // Add storage info.
        int storageSize = 256;
        websitePreferenceBridge.addStorageInfo(new StorageInfo(googleOrigin, 0, storageSize));

        // Add local storage info.
        websitePreferenceBridge.addLocalStorageInfoMapEntry(
                new LocalStorageInfo(googleOrigin, storageSize, false));

        // Add chooser info types.
        websitePreferenceBridge.addChosenObjectInfo(
                new ChosenObjectInfo(ContentSettingsType.USB_CHOOSER_DATA, googleOrigin,
                        googleOrigin, "Gadget", "Object", false));

        fetcher.fetchAllPreferences((sites) -> {
            Assert.assertEquals(1, sites.size());
            Website site = sites.iterator().next();

            Assert.assertTrue(site.getAddress().matches(googleOrigin));

            // Check permission info types for |site|.
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.GEOLOCATION));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.MIDI));
            Assert.assertNotNull(
                    site.getPermissionInfo(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.NOTIFICATION));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.CAMERA));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.MICROPHONE));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.CLIPBOARD));
            Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.SENSORS));

            // Check content setting exception types.
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.COOKIE));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.POPUP));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.ADS));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.JAVASCRIPT));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.SOUND));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.BACKGROUND_SYNC));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(
                            ContentSettingException.Type.AUTOMATIC_DOWNLOADS));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSettingPermission(ContentSettingException.Type.AUTOPLAY));

            // Check storage info.
            ArrayList<StorageInfo> storageInfos = new ArrayList<>(site.getStorageInfo());
            Assert.assertEquals(1, storageInfos.size());

            StorageInfo storageInfo = storageInfos.get(0);
            Assert.assertEquals(googleOrigin, storageInfo.getHost());
            Assert.assertEquals(storageSize, storageInfo.getSize());

            // Check local storage info.
            LocalStorageInfo localStorageInfo = site.getLocalStorageInfo();
            Assert.assertEquals(googleOrigin, localStorageInfo.getOrigin());
            Assert.assertEquals(storageSize, localStorageInfo.getSize());
            Assert.assertFalse(localStorageInfo.isDomainImportant());

            // Check chooser info types.
            ArrayList<ChosenObjectInfo> chosenObjectInfos =
                    new ArrayList<>(site.getChosenObjectInfo());
            Assert.assertEquals(1, chosenObjectInfos.size());
            Assert.assertEquals(ContentSettingsType.USB_CHOOSER_DATA,
                    chosenObjectInfos.get(0).getContentSettingsType());
        });
    }

    @Test
    @SmallTest
    public void testFetchAllPreferencesForMultipleOrigins() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        String chromiumOrigin = "https://chromium.org";
        String exampleOrigin = "https://example.com";

        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.GEOLOCATION, googleOrigin, googleOrigin, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.GEOLOCATION, chromiumOrigin, chromiumOrigin, false));

        Website expectedGoogleWebsite =
                new Website(WebsiteAddress.create(googleOrigin), WebsiteAddress.create(null));
        Website expectedChromiumWebsite =
                new Website(WebsiteAddress.create(chromiumOrigin), WebsiteAddress.create(null));

        fetcher.fetchAllPreferences((sites) -> {
            Assert.assertEquals(2, sites.size());

            // The order of |sites| is unknown, so check if the array contains a geolocation
            // permission for each of the sites.
            ArrayList<Website> siteArray = new ArrayList<>(sites);
            boolean containsGoogleOriginPermission = false;
            boolean containsChromiumOriginPermission = false;
            for (Website site : siteArray) {
                if (site.compareByAddressTo(expectedGoogleWebsite) == 0) {
                    containsGoogleOriginPermission = true;
                } else if (site.compareByAddressTo(expectedChromiumWebsite) == 0) {
                    containsChromiumOriginPermission = true;
                }

                Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.GEOLOCATION));
            }

            Assert.assertTrue(containsGoogleOriginPermission);
            Assert.assertTrue(containsChromiumOriginPermission);
        });

        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                PermissionInfo.Type.GEOLOCATION, exampleOrigin, exampleOrigin, false));

        Website expectedExampleWebsite =
                new Website(WebsiteAddress.create(exampleOrigin), WebsiteAddress.create(null));

        fetcher.fetchAllPreferences((sites) -> {
            Assert.assertEquals(3, sites.size());

            ArrayList<Website> siteArray = new ArrayList<>(sites);
            boolean containsGoogleOriginPermission = false;
            boolean containsChromiumOriginPermission = false;
            boolean containsExampleOriginPermission = false;
            for (Website site : siteArray) {
                if (site.compareByAddressTo(expectedGoogleWebsite) == 0) {
                    containsGoogleOriginPermission = true;
                } else if (site.compareByAddressTo(expectedChromiumWebsite) == 0) {
                    containsChromiumOriginPermission = true;
                } else if (site.compareByAddressTo(expectedExampleWebsite) == 0) {
                    containsExampleOriginPermission = true;
                }

                Assert.assertNotNull(site.getPermissionInfo(PermissionInfo.Type.GEOLOCATION));
            }

            Assert.assertTrue(containsGoogleOriginPermission);
            Assert.assertTrue(containsChromiumOriginPermission);
            Assert.assertTrue(containsExampleOriginPermission);
        });
    }

    public void assertContentSettingExceptionEquals(
            ContentSettingException expected, ContentSettingException actual) {
        Assert.assertEquals(expected.getSource(), actual.getSource());
        Assert.assertEquals(expected.getPattern(), actual.getPattern());
        Assert.assertEquals(expected.getContentSetting(), actual.getContentSetting());
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryPermissionInfoTypes() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        ArrayList<Integer> permissionInfoTypes = new ArrayList<>(Arrays.asList(
                PermissionInfo.Type.CAMERA, PermissionInfo.Type.CLIPBOARD,
                PermissionInfo.Type.GEOLOCATION, PermissionInfo.Type.MICROPHONE,
                PermissionInfo.Type.NOTIFICATION, PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER,
                PermissionInfo.Type.SENSORS));

        for (@PermissionInfo.Type int type : permissionInfoTypes) {
            PermissionInfo fakePermissionInfo =
                    new PermissionInfo(type, googleOrigin, googleOrigin, false);
            websitePreferenceBridge.addPermissionInfo(fakePermissionInfo);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromContentSettingsType(
                            PermissionInfo.getContentSettingsType(type)),
                    (sites) -> {
                        Assert.assertEquals(1, sites.size());

                        Website site = sites.iterator().next();
                        Assert.assertNotNull(site.getPermissionInfo(type));
                    });
        }
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryContentSettingExceptionTypes() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        String preferenceSource = "preference";
        ArrayList<Integer> contentSettingExceptionTypes = new ArrayList<>(Arrays.asList(
                ContentSettingException.Type.ADS, ContentSettingException.Type.AUTOMATIC_DOWNLOADS,
                ContentSettingException.Type.AUTOPLAY, ContentSettingException.Type.BACKGROUND_SYNC,
                ContentSettingException.Type.COOKIE, ContentSettingException.Type.JAVASCRIPT,
                ContentSettingException.Type.POPUP, ContentSettingException.Type.SOUND));

        for (@ContentSettingsType int type : contentSettingExceptionTypes) {
            @ContentSettingsType
            int contentSettingsType = ContentSettingException.getContentSettingsType(type);
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(contentSettingsType, googleOrigin,
                                ContentSettingValues.DEFAULT, preferenceSource);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(contentSettingsType),
                        (sites) -> {
                            Assert.assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(fakeContentSettingException,
                                    site.getContentSettingException(type));
                        });
            }

            // Make sure that the content setting value is updated.
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(contentSettingsType, googleOrigin,
                                ContentSettingValues.BLOCK, preferenceSource);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(contentSettingsType),
                        (sites) -> {
                            Assert.assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(fakeContentSettingException,
                                    site.getContentSettingException(type));
                        });
            }
        }
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryStorageInfo() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        String chromiumOrigin = "https://chromium.org";
        int storageSize = 256;
        StorageInfo fakeStorageInfo = new StorageInfo(googleOrigin, 0, storageSize);
        LocalStorageInfo fakeLocalStorageInfo =
                new LocalStorageInfo(googleOrigin, storageSize, false);
        LocalStorageInfo fakeImportantLocalStorageInfo =
                new LocalStorageInfo(chromiumOrigin, storageSize, true);

        websitePreferenceBridge.addStorageInfo(fakeStorageInfo);
        websitePreferenceBridge.addLocalStorageInfoMapEntry(fakeLocalStorageInfo);
        websitePreferenceBridge.addLocalStorageInfoMapEntry(fakeImportantLocalStorageInfo);

        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.USE_STORAGE),
                (sites) -> {
                    Assert.assertEquals(1, sites.size());

                    Website site = sites.iterator().next();
                    List<StorageInfo> storageInfos = site.getStorageInfo();
                    Assert.assertEquals(1, storageInfos.size());

                    StorageInfo storageInfo = storageInfos.get(0);
                    Assert.assertEquals(fakeStorageInfo.getSize(), storageInfo.getSize());
                    Assert.assertEquals(fakeStorageInfo.getHost(), storageInfo.getHost());

                    LocalStorageInfo localStorageInfo = site.getLocalStorageInfo();
                    Assert.assertFalse(localStorageInfo.isDomainImportant());
                    Assert.assertEquals(fakeLocalStorageInfo.getSize(), localStorageInfo.getSize());
                    Assert.assertEquals(
                            fakeLocalStorageInfo.getOrigin(), localStorageInfo.getOrigin());
                });

        // Test that the fetcher gets local storage info for important domains.
        fetcher = new WebsitePermissionsFetcher(true);
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(SiteSettingsCategory.Type.USE_STORAGE),
                (sites) -> {
                    Assert.assertEquals(2, sites.size());

                    for (Website site : sites) {
                        if (site.getAddress().matches(googleOrigin)) {
                            List<StorageInfo> storageInfos = site.getStorageInfo();
                            Assert.assertEquals(1, storageInfos.size());

                            StorageInfo storageInfo = storageInfos.get(0);
                            Assert.assertEquals(fakeStorageInfo.getSize(), storageInfo.getSize());
                            Assert.assertEquals(fakeStorageInfo.getHost(), storageInfo.getHost());

                            Assert.assertNull(site.getLocalStorageInfo());
                        } else if (site.getAddress().matches(chromiumOrigin)) {
                            List<StorageInfo> storageInfos = site.getStorageInfo();
                            Assert.assertEquals(0, storageInfos.size());

                            LocalStorageInfo localStorageInfo = site.getLocalStorageInfo();
                            Assert.assertTrue(localStorageInfo.isDomainImportant());
                            Assert.assertEquals(fakeImportantLocalStorageInfo.getSize(),
                                    localStorageInfo.getSize());
                            Assert.assertEquals(fakeImportantLocalStorageInfo.getOrigin(),
                                    localStorageInfo.getOrigin());
                        } else {
                            Assert.fail("The WebsitePermissionsFetcher should only return "
                                    + "Website objects for the granted origins.");
                        }
                    }
                });
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryChooserDataTypes() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        ArrayList<Integer> chooserDataTypes =
                new ArrayList<>(Arrays.asList(SiteSettingsCategory.Type.USB));

        for (@SiteSettingsCategory.Type int type : chooserDataTypes) {
            @ContentSettingsType
            int chooserDataType = SiteSettingsCategory.objectChooserDataTypeFromGuard(
                    SiteSettingsCategory.contentSettingsType(type));
            Assert.assertNotEquals(-1, chooserDataType);

            ChosenObjectInfo fakeObjectInfo = new ChosenObjectInfo(chooserDataType, googleOrigin,
                    googleOrigin, "Chosen Object", "SerializedObjectData", false);
            websitePreferenceBridge.addChosenObjectInfo(fakeObjectInfo);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromType(type), (sites) -> {
                        Assert.assertEquals(1, sites.size());

                        List<ChosenObjectInfo> objectInfos =
                                new ArrayList<>(sites.iterator().next().getChosenObjectInfo());
                        Assert.assertEquals(1, objectInfos.size());
                        Assert.assertEquals(fakeObjectInfo, objectInfos.get(0));
                    });
        }
    }
}
