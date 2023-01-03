// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import static java.util.Map.entry;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.ChosenObjectInfo;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.LocalStorageInfo;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.StorageInfo;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for WebsitePermissionsFetcher.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        WebsitePermissionsFetcherTest.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
        WebsitePermissionsFetcherTest.ENABLE_WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND})
@Batch(Batch.PER_CLASS)
public class WebsitePermissionsFetcherTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private SiteSettingsDelegate mSiteSettingsDelegate;

    /** Command line flag to enable experimental web platform features in tests. */
    public static final String ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =
            "enable-experimental-web-platform-features";

    /**
     * Command line flag to enable the new Web Bluetooth permissions backend in
     * tests.
     */
    public static final String ENABLE_WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND =
            "enable-features=WebBluetoothNewPermissionsBackend";

    private static final BrowserContextHandle UNUSED_BROWSER_CONTEXT_HANDLE = null;

    private static final String[] PERMISSION_URLS = {
            "http://www.google.com/",
            "http://www.youtube.com/",
            "http://www.facebook.com/",
            "http://www.baidu.com/",
            "http://www.wikipedia.org/",
            "http://www.yahoo.com/",
            "http://www.google.co.in/",
            "http://www.reddit.com/",
            "http://www.qq.com/",
            "http://www.taobao.com/",
            "http://www.google.co.jp/",
            "http://www.amazon.com/",
            "http://www.twitter.com/",
            "http://www.live.com/",
            "http://www.instagram.com/",
            "http://www.weibo.com/",
            "http://www.google.de/",
            "http://www.google.co.uk/",
            "http://www.google.com.br/",
            "http://www.google.fr/",
            "http://www.google.ru/",
            "http://www.linkedin.com/",
            "http://www.google.com.hk/",
            "http://www.yandex.ru/",
            "http://www.google.it/",
            "http://www.netflix.com/",
            "http://www.yahoo.co.jp/",
            "http://www.google.es/",
            "http://www.t.co/",
            "http://www.google.com.mx/",
            "http://www.google.ca/",
            "http://www.ebay.com/",
            "http://www.alipay.com/",
            "http://www.bing.com/",
            "http://www.imgur.com/",
            "http://www.twitch.tv/",
            "http://www.msn.com/",
            "http://www.apple.com/",
            "http://www.aliexpress.com/",
            "http://www.microsoft.com/",
            "http://www.wordpress.com/",
            "http://www.office.com/",
            "http://www.mail.ru/",
            "http://www.tumblr.com/",
            "http://www.stackoverflow.com/",
            "http://www.microsoftonline.com/",
            "http://www.imdb.com/",
            "http://www.github.com/",
            "http://www.blogspot.com/",
            "http://www.amazon.co.jp/",
            "http://www.google.com.au/",
            "http://www.google.com.tw/",
            "http://www.google.com.tr/",
            "http://www.paypal.com/",
            "http://www.google.pl/",
            "http://www.wikia.com/",
            "http://www.pinterest.com/",
            "http://www.whatsapp.com/",
            "http://www.google.co.id/",
            "http://www.espn.com/",
            "http://www.adobe.com/",
            "http://www.google.com.ar/",
            "http://www.googleusercontent.com/",
            "http://www.amazon.in/",
            "http://www.dropbox.com/",
            "http://www.amazon.de/",
            "http://www.google.com.ua/",
            "http://www.so.com/",
            "http://www.google.com.pk/",
            "http://www.cnn.com/",
            "http://www.amazon.co.uk/",
            "http://www.bbc.co.uk/",
            "http://www.google.com.sa/",
            "http://www.craigslist.org/",
            "http://www.bbc.com/",
            "http://www.google.co.th/",
            "http://www.google.com.eg/",
            "http://www.google.nl/",
            "http://www.amazonaws.com/",
            "http://www.soundcloud.com/",
            "http://www.ask.com/",
            "http://www.google.co.za/",
            "http://www.booking.com/",
            "http://www.nytimes.com/",
            "http://www.google.co.ve/",
            "http://www.google.co.kr/",
            "http://www.quora.com/",
            "http://www.mozilla.org/",
            "http://www.dailymotion.com/",
            "http://www.stackexchange.com/",
            "http://www.salesforce.com/",
            "http://www.detik.com/",
            "http://www.blogger.com/",
            "http://www.ebay.de/",
            "http://www.vimeo.com/",
            "http://www.theguardian.com/",
            "http://www.tribunnews.com/",
            "http://www.google.com.sg/",
            "http://www.google.gr/",
            "http://www.flipkart.com/",
            "http://www.spotify.com/",
            "http://www.slideshare.net/",
            "http://www.chase.com/",
            "http://www.google.com.ph/",
            "http://www.ebay.co.uk/",
            "http://www.google.se/",
            "http://www.cnet.com/",
            "http://www.google.be/",
            "http://www.nih.gov/",
            "http://www.google.cn/",
            "http://www.steamcommunity.com/",
            "http://www.mediafire.com/",
            "http://www.xinhuanet.com/",
            "http://www.google.az/",
            "http://www.vice.com/",
            "http://www.alibaba.com/",
            "http://www.dailymail.co.uk/",
            "http://www.google.com.co/",
            "http://www.buzzfeed.com/",
            "http://www.doubleclick.net/",
            "http://www.google.com.ng/",
            "http://www.google.co.ao/",
            "http://www.google.at/",
            "http://www.uol.com.br/",
            "http://www.redd.it/",
            "http://www.deviantart.com/",
            "http://www.washingtonpost.com/",
            "http://www.twimg.com/",
            "http://www.w3schools.com/",
            "http://www.godaddy.com/",
            "http://www.wikihow.com/",
            "http://www.etsy.com/",
            "http://www.slack.com/",
            "http://www.google.cz/",
            "http://www.google.ch/",
            "http://www.amazon.it/",
            "http://www.steampowered.com/",
            "http://www.google.com.vn/",
            "http://www.walmart.com/",
            "http://www.messenger.com/",
            "http://www.discordapp.com/",
            "http://www.google.cl/",
            "http://www.amazon.fr/",
            "http://www.yelp.com/",
            "http://www.google.no/",
            "http://www.google.pt/",
            "http://www.google.ae/",
            "http://www.weather.com/",
            "http://www.mercadolivre.com.br/",
            "http://www.google.ro/",
            "http://www.bankofamerica.com/",
            "http://www.blogspot.co.id/",
            "http://www.trello.com/",
            "http://www.gfycat.com/",
            "http://www.ikea.com/",
            "http://www.scribd.com/",
            "http://www.china.com.cn/",
            "http://www.forbes.com/",
            "http://www.wellsfargo.com/",
            "http://www.indiatimes.com/",
            "http://www.cnblogs.com/",
            "http://www.gamepedia.com/",
            "http://www.outbrain.com/",
            "http://www.9gag.com/",
            "http://www.google.ie/",
            "http://www.gearbest.com/",
            "http://www.files.wordpress.com/",
            "http://www.huffingtonpost.com/",
            "http://www.speedtest.net/",
            "http://www.google.dk/",
            "http://www.google.fi/",
            "http://www.wikimedia.org/",
            "http://www.thesaurus.com/",
            "http://www.livejournal.com/",
            "http://www.nfl.com/",
            "http://www.zillow.com/",
            "http://www.foxnews.com/",
            "http://www.researchgate.net/",
            "http://www.amazon.cn/",
            "http://www.google.hu/",
            "http://www.google.co.il/",
            "http://www.amazon.es/",
            "http://www.wordreference.com/",
            "http://www.blackboard.com/",
            "http://www.google.dz/",
            "http://www.tripadvisor.com/",
            "http://www.shutterstock.com/",
            "http://www.cloudfront.net/",
            "http://www.aol.com/",
            "http://www.weebly.com/",
            "http://www.battle.net/",
            "http://www.archive.org/",
    };

    private static final Map<String, String> FPS_MEMBER_TO_OWNER_MAP = Map.ofEntries(
            entry("https://google.de", "google.com"), entry("https://youtube.com", "google.com"),
            entry("https://google.ch", "google.com"), entry("https://google.it", "google.com"),
            entry("https://wikipedia.org", "wikipedia.org"),
            entry("https://chromium.org", "chromium.org"),
            entry("https://googlesource.org", "chromium.org"),
            entry("https://verizon.com", "verizon.com"),
            entry("https://verizonconnect.com", "verizon.com"),
            entry("https://aol.com", "verizon.com"), entry("https://vodafone.de", "vodafone.com"));

    private static class WebsitePermissionsWaiter
            extends CallbackHelper implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            notifyCalled();
        }
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clean up permissions.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.SITE_SETTINGS}, TimePeriod.ALL_TIME);
        });
        helper.waitForCallback(0);
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
    @DisabledTest(message = "https://crbug.com/1085592")
    public void testFetcherDoesNotTimeOutWithManyUrls() throws Exception {
        final WebsitePermissionsWaiter waiter = new WebsitePermissionsWaiter();
        // Set lots of permissions values.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            for (String url : PERMISSION_URLS) {
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.GEOLOCATION, url, url, ContentSettingValues.BLOCK);
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.MIDI_SYSEX, url, url, ContentSettingValues.ALLOW);
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, url, url,
                        ContentSettingValues.BLOCK);
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.NOTIFICATIONS, url, url, ContentSettingValues.ALLOW);
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.MEDIASTREAM_MIC, url, url, ContentSettingValues.ALLOW);
                WebsitePreferenceBridgeJni.get().setPermissionSettingForOrigin(profile,
                        ContentSettingsType.MEDIASTREAM_CAMERA, url, url,
                        ContentSettingValues.BLOCK);
            }

            // This should not time out. See crbug.com/732907.
            WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(profile);
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
        public List<PermissionInfo> getPermissionInfo(
                BrowserContextHandle browserContextHandle, @ContentSettingsType int type) {
            List<PermissionInfo> result = new ArrayList<>();
            for (PermissionInfo info : mPermissionInfos) {
                if (info.getContentSettingsType() == type) {
                    result.add(info);
                }
            }
            return result;
        }

        @Override
        public List<ContentSettingException> getContentSettingsExceptions(
                BrowserContextHandle browserContextHandle,
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
        public void fetchLocalStorageInfo(BrowserContextHandle browserContextHandle,
                Callback<HashMap> callback, boolean fetchImportant) {
            if (fetchImportant) {
                callback.onResult(mImportantLocalStorageInfoMap);
                return;
            }
            callback.onResult(mLocalStorageInfoMap);
        }

        @Override
        public void fetchStorageInfo(
                BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
            callback.onResult(mStorageInfos);
        }

        @Override
        public List<ChosenObjectInfo> getChosenObjectInfo(
                BrowserContextHandle browserContextHandle, int contentSettingsType) {
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

        public void resetContentSettingExceptions() {
            mContentSettingExceptions.clear();
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
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";

        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(ContentSettingsType.AR, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.IDLE_DETECTION, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.GEOLOCATION, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.MIDI_SYSEX, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, googleOrigin,
                        SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(ContentSettingsType.NFC, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.NOTIFICATIONS, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.MEDIASTREAM_CAMERA, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.MEDIASTREAM_MIC, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.CLIPBOARD_READ_WRITE, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.SENSORS, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(ContentSettingsType.VR, googleOrigin, SITE_WILDCARD, false));

        // Add content setting exception types.
        String preferenceSource = "preference";
        // If the ContentSettingsType.NUM_TYPES value changes *and* a new value has been exposed on
        // Android, then please update this code block to include a test for your new type.
        // Otherwise, just update count in the assert.
        Assert.assertEquals(83, ContentSettingsType.NUM_TYPES);
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.COOKIES, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.POPUPS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.ADS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.JAVASCRIPT, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.SOUND, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.BACKGROUND_SYNC, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.AUTOMATIC_DOWNLOADS, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(new ContentSettingException(
                ContentSettingsType.INSECURE_PRIVATE_NETWORK, googleOrigin,
                ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.JAVASCRIPT_JIT, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.AUTO_DARK_WEB_CONTENT, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(ContentSettingsType.REQUEST_DESKTOP_SITE, googleOrigin,
                        ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));
        websitePreferenceBridge.addContentSettingException(new ContentSettingException(
                ContentSettingsType.FEDERATED_IDENTITY_API, googleOrigin,
                ContentSettingValues.DEFAULT, preferenceSource, /*isEmbargoed=*/false));

        // Add storage info.
        int storageSize = 256;
        websitePreferenceBridge.addStorageInfo(new StorageInfo(googleOrigin, 0, storageSize));

        // Add local storage info.
        websitePreferenceBridge.addLocalStorageInfoMapEntry(
                new LocalStorageInfo(googleOrigin, storageSize, false));

        // Add chooser info types.
        websitePreferenceBridge.addChosenObjectInfo(new ChosenObjectInfo(
                ContentSettingsType.USB_CHOOSER_DATA, googleOrigin, "Gadget", "Object", false));
        websitePreferenceBridge.addChosenObjectInfo(
                new ChosenObjectInfo(ContentSettingsType.BLUETOOTH_CHOOSER_DATA, googleOrigin,
                        "Wireless", "Object", false));

        fetcher.fetchAllPreferences((sites) -> {
            Assert.assertEquals(1, sites.size());
            Website site = sites.iterator().next();

            Assert.assertTrue(site.getAddress().matches(googleOrigin));

            // Check permission info types for |site|.
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.IDLE_DETECTION));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.MIDI_SYSEX));
            Assert.assertNotNull(
                    site.getPermissionInfo(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.NOTIFICATIONS));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.MEDIASTREAM_CAMERA));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.MEDIASTREAM_MIC));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.CLIPBOARD_READ_WRITE));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.SENSORS));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.VR));
            Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.AR));

            // Check content setting exception types.
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.COOKIES));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.POPUPS));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.ADS));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.JAVASCRIPT));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.SOUND));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.BACKGROUND_SYNC));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(UNUSED_BROWSER_CONTEXT_HANDLE,
                            ContentSettingsType.AUTOMATIC_DOWNLOADS));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(
                            UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.JAVASCRIPT_JIT));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(UNUSED_BROWSER_CONTEXT_HANDLE,
                            ContentSettingsType.AUTO_DARK_WEB_CONTENT));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(UNUSED_BROWSER_CONTEXT_HANDLE,
                            ContentSettingsType.REQUEST_DESKTOP_SITE));
            Assert.assertEquals(Integer.valueOf(ContentSettingValues.DEFAULT),
                    site.getContentSetting(UNUSED_BROWSER_CONTEXT_HANDLE,
                            ContentSettingsType.FEDERATED_IDENTITY_API));

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
            Assert.assertEquals(2, chosenObjectInfos.size());
            Assert.assertEquals(ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
                    chosenObjectInfos.get(0).getContentSettingsType());
            Assert.assertEquals(ContentSettingsType.USB_CHOOSER_DATA,
                    chosenObjectInfos.get(1).getContentSettingsType());
        });
    }

    @Test
    @SmallTest
    public void testFetchAllPreferencesForMultipleOrigins() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        String chromiumOrigin = "https://chromium.org";
        String exampleOrigin = "https://example.com";

        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.GEOLOCATION, googleOrigin, SITE_WILDCARD, false));
        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.GEOLOCATION, chromiumOrigin, SITE_WILDCARD, false));

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

                Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
            }

            Assert.assertTrue(containsGoogleOriginPermission);
            Assert.assertTrue(containsChromiumOriginPermission);
        });

        websitePreferenceBridge.addPermissionInfo(new PermissionInfo(
                ContentSettingsType.GEOLOCATION, exampleOrigin, SITE_WILDCARD, false));

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

                Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
            }

            Assert.assertTrue(containsGoogleOriginPermission);
            Assert.assertTrue(containsChromiumOriginPermission);
            Assert.assertTrue(containsExampleOriginPermission);
        });
    }

    public void assertContentSettingExceptionEquals(
            ContentSettingException expected, ContentSettingException actual) {
        Assert.assertEquals(expected.getSource(), actual.getSource());
        Assert.assertEquals(expected.isEmbargoed(), actual.isEmbargoed());
        Assert.assertEquals(expected.getPrimaryPattern(), actual.getPrimaryPattern());
        Assert.assertEquals(expected.getSecondaryPattern(), actual.getSecondaryPattern());
        Assert.assertEquals(expected.getContentSetting(), actual.getContentSetting());
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryPermissionInfoTypes() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        // MIDI is excluded from this list because it does not have a top level category.
        ArrayList<Integer> permissionInfoTypes = new ArrayList<>(
                Arrays.asList(ContentSettingsType.AR, ContentSettingsType.MEDIASTREAM_CAMERA,
                        ContentSettingsType.CLIPBOARD_READ_WRITE, ContentSettingsType.GEOLOCATION,
                        ContentSettingsType.IDLE_DETECTION, ContentSettingsType.MEDIASTREAM_MIC,
                        ContentSettingsType.NFC, ContentSettingsType.NOTIFICATIONS,
                        ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER, ContentSettingsType.SENSORS,
                        ContentSettingsType.VR));

        for (@ContentSettingsType int type : permissionInfoTypes) {
            PermissionInfo fakePermissionInfo =
                    new PermissionInfo(type, googleOrigin, SITE_WILDCARD, false);
            websitePreferenceBridge.addPermissionInfo(fakePermissionInfo);

            fetcher.fetchPreferencesForCategory(SiteSettingsCategory.createFromContentSettingsType(
                                                        UNUSED_BROWSER_CONTEXT_HANDLE, type),
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
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleOrigin = "https://google.com";
        String preferenceSource = "preference";
        boolean isEmbargoed = false;
        ArrayList<Integer> contentSettingExceptionTypes = new ArrayList<>(
                Arrays.asList(ContentSettingsType.ADS, ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        ContentSettingsType.BACKGROUND_SYNC, ContentSettingsType.BLUETOOTH_SCANNING,
                        ContentSettingsType.COOKIES, ContentSettingsType.FEDERATED_IDENTITY_API,
                        ContentSettingsType.JAVASCRIPT, ContentSettingsType.POPUPS,
                        ContentSettingsType.SOUND));

        for (@ContentSettingsType int type : contentSettingExceptionTypes) {
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(type, googleOrigin,
                                ContentSettingValues.DEFAULT, preferenceSource, isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, type),
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
                        new ContentSettingException(type, googleOrigin, ContentSettingValues.BLOCK,
                                preferenceSource, isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, type),
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
    public void testFetchPreferencesForAdvancedCookieSettings() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String mainSite = "https://a.com";
        String thirdPartySite = "https://b.com";
        String preferenceSource = "preference";
        boolean isEmbargoed = false;
        @ContentSettingsType
        int contentSettingsType = ContentSettingsType.COOKIES;

        // Test the advanced exception combinations of:
        // b.com on a.com
        // a.com on a.com
        // * on a.com
        ArrayList<Pair<String, String>> exceptions =
                new ArrayList<>(Arrays.asList(new Pair<>(thirdPartySite, mainSite),
                        new Pair<>(mainSite, mainSite), new Pair<>(SITE_WILDCARD, mainSite)));

        for (Pair<String, String> pair : exceptions) {
            websitePreferenceBridge.resetContentSettingExceptions();
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(contentSettingsType, pair.first, pair.second,
                                ContentSettingValues.DEFAULT, preferenceSource, isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, contentSettingsType),
                        (sites) -> {
                            Assert.assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(fakeContentSettingException,
                                    site.getContentSettingException(contentSettingsType));
                        });
            }

            // Make sure that the content setting value is updated.
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(contentSettingsType, pair.first, pair.second,
                                ContentSettingValues.BLOCK, preferenceSource, isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, contentSettingsType),
                        (sites) -> {
                            Assert.assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(fakeContentSettingException,
                                    site.getContentSettingException(contentSettingsType));
                        });
            }
        }
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryStorageInfo() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
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
                SiteSettingsCategory.createFromType(
                        UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.USE_STORAGE),
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
        fetcher = new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE, true);
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(
                        UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.USE_STORAGE),
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
        String googleOrigin = "https://google.com";
        ArrayList<Integer> chooserDataTypes = new ArrayList<>(
                Arrays.asList(SiteSettingsCategory.Type.USB, SiteSettingsCategory.Type.BLUETOOTH));

        for (@SiteSettingsCategory.Type int type : chooserDataTypes) {
            WebsitePermissionsFetcher fetcher =
                    new WebsitePermissionsFetcher(UNUSED_BROWSER_CONTEXT_HANDLE);
            FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
            fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
            @ContentSettingsType
            int chooserDataType = SiteSettingsCategory.objectChooserDataTypeFromGuard(
                    SiteSettingsCategory.contentSettingsType(type));
            Assert.assertNotEquals(-1, chooserDataType);

            ChosenObjectInfo fakeObjectInfo = new ChosenObjectInfo(
                    chooserDataType, googleOrigin, "Chosen Object", "SerializedObjectData", false);
            websitePreferenceBridge.addChosenObjectInfo(fakeObjectInfo);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromType(UNUSED_BROWSER_CONTEXT_HANDLE, type),
                    (sites) -> {
                        Assert.assertEquals(1, sites.size());

                        List<ChosenObjectInfo> objectInfos =
                                new ArrayList<>(sites.iterator().next().getChosenObjectInfo());
                        Assert.assertEquals(1, objectInfos.size());
                        Assert.assertEquals(fakeObjectInfo, objectInfos.get(0));
                    });
        }
    }

    @Test
    @SmallTest
    public void testGetFirstPartySetsOwnersAndMergeInfoIntoWebsites() {
        MockitoAnnotations.initMocks(this);

        for (var entry : FPS_MEMBER_TO_OWNER_MAP.entrySet()) {
            Mockito.doReturn(entry.getValue())
                    .when(mSiteSettingsDelegate)
                    .getFirstPartySetOwner(entry.getKey());
        }

        Mockito.doReturn(true).when(mSiteSettingsDelegate).isFirstPartySetsDataAccessEnabled();
        Mockito.doReturn(true)
                .when(mSiteSettingsDelegate)
                .isPrivacySandboxFirstPartySetsUIFeatureEnabled();

        var fetcher = new WebsitePermissionsFetcher(
                UNUSED_BROWSER_CONTEXT_HANDLE, /* fetchSiteImportantInfo= */ false);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleDEOrigin = "https://google.de";
        String googleITOrigin = "https://google.it";
        String googleCHOrigin = "https://google.ch";
        String youtubeOrigin = "https://youtube.com";
        String verizonConnectOrigin = "https://verizonconnect.com";
        String aolOrigin = "https://aol.com";
        String noInFPSOrigin = "https://unknow.ch";

        Website expectedYoutubeWebsite =
                new Website(WebsiteAddress.create(youtubeOrigin), WebsiteAddress.create(null));
        Website expectedVerizonConnectWebsite = new Website(
                WebsiteAddress.create(verizonConnectOrigin), WebsiteAddress.create(null));
        Website expectedNoInFPSWebsite =
                new Website(WebsiteAddress.create(noInFPSOrigin), WebsiteAddress.create(null));

        // Use a list of origins and create content settings exceptions.
        List<String> origins = Arrays.asList(googleDEOrigin, googleITOrigin, googleCHOrigin,
                youtubeOrigin, verizonConnectOrigin, aolOrigin, noInFPSOrigin);
        // Adding content exceptions will generate websites data.
        for (String origin : origins) {
            websitePreferenceBridge.addContentSettingException(
                    new ContentSettingException(ContentSettingsType.COOKIES, origin,
                            ContentSettingValues.ALLOW, "preference", /*isEmbargoed=*/false));
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            fetcher.fetchPreferencesForCategoryAndPopulateFpsInfo(mSiteSettingsDelegate,
                    SiteSettingsCategory.createFromType(
                            UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.ALL_SITES),
                    (sites) -> {
                        // Verify the number of sites is the same of the origins with exceptions.
                        Assert.assertEquals(origins.size(), sites.size());

                        ArrayList<Website> siteArray = new ArrayList<>(sites);
                        for (Website site : siteArray) {
                            // Verify youtube.com has google.com as FPS owner which has 4 members
                            // within the group of sites with data.
                            if (site.compareByAddressTo(expectedYoutubeWebsite) == 0) {
                                Assert.assertNotNull(site.getFPSCookieInfo());
                                Assert.assertEquals(
                                        "google.com", site.getFPSCookieInfo().getOwner());
                                Assert.assertEquals(4, site.getFPSCookieInfo().getMembersCount());
                            }
                            // Verify verizonconnect.com has verizon.com as FPS owner which has 2
                            // members within the group of sites with data.
                            if (site.compareByAddressTo(expectedVerizonConnectWebsite) == 0) {
                                Assert.assertNotNull(site.getFPSCookieInfo());
                                Assert.assertEquals(
                                        "verizon.com", site.getFPSCookieInfo().getOwner());
                                Assert.assertEquals(2, site.getFPSCookieInfo().getMembersCount());
                            }

                            // Verify a website with data which is not in a FPS has no FPS data set.
                            if (site.compareByAddressTo(expectedNoInFPSWebsite) == 0) {
                                Assert.assertEquals(null, site.getFPSCookieInfo());
                            }
                        }
                    });
        });
    }
}
