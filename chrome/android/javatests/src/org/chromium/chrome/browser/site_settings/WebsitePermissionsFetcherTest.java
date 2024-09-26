// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import static java.util.Map.entry;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.site_settings.ChosenObjectInfo;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.CookiesInfo;
import org.chromium.components.browser_ui.site_settings.LocalStorageInfo;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.SharedDictionaryInfo;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.StorageInfo;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browsing_data.content.BrowsingDataInfo;
import org.chromium.components.browsing_data.content.BrowsingDataModel;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for WebsitePermissionsFetcher. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    WebsitePermissionsFetcherTest.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    WebsitePermissionsFetcherTest.ENABLE_WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND
})
@Batch(Batch.PER_CLASS)
public class WebsitePermissionsFetcherTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    @Mock private BrowsingDataModel mBrowsingDataModel;

    /** Command line flag to enable experimental web platform features in tests. */
    public static final String ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =
            "enable-experimental-web-platform-features";

    /** Command line flag to enable the new Web Bluetooth permissions backend in tests. */
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

    private static final Map<String, String> RWS_MEMBER_TO_OWNER_MAP =
            Map.ofEntries(
                    entry("https://google.de", "google.com"),
                    entry("https://youtube.com", "google.com"),
                    entry("https://google.ch", "google.com"),
                    entry("https://google.it", "google.com"),
                    entry("https://wikipedia.org", "wikipedia.org"),
                    entry("https://chromium.org", "chromium.org"),
                    entry("https://googlesource.org", "chromium.org"),
                    entry("https://verizon.com", "verizon.com"),
                    entry("https://verizonconnect.com", "verizon.com"),
                    entry("https://aol.com", "verizon.com"),
                    entry("https://vodafone.de", "vodafone.com"));

    private static final List<Integer> EMBEDDED_CONTENT_SETTINGS =
            Arrays.asList(ContentSettingsType.STORAGE_ACCESS);

    private static final String ORIGIN = "https://google.com:443";
    private static final String EMBEDDER = "https://embedder.com";
    private static final int EXPIRATION_IN_DAYS = 30;

    public static class EmbargoedParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(true).name("Embargoed"),
                    new ParameterSet().value(false).name("Normal"));
        }
    }

    public static class EmbargoedAndOneTimeSessionParameters implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(false, false).name("NormalDurable"),
                    new ParameterSet().value(true, false).name("EmbargoedDurable"),
                    new ParameterSet().value(false, true).name("NormalOneTime"),
                    new ParameterSet().value(true, true).name("EmbargoedOneTime"));
        }
    }

    public static class BrowsingDataModelEnabled implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(true).name("BDMEnabled"),
                    new ParameterSet().value(false).name("BDMDisabled"));
        }
    }

    private static class WebsitePermissionsWaiter extends CallbackHelper
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {

        private Collection<Website> mSites;

        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            mSites = sites;
            notifyCalled();
        }

        public Collection<Website> getSites() {
            return mSites;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clean up permissions.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
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

        assertEquals("nullBoth", map.get(nullBoth));
        assertEquals("nullOrigin", map.get(nullOrigin));
        assertEquals("nullEmbedder", map.get(nullEmbedder));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1085592")
    public void testFetcherDoesNotTimeOutWithManyUrls() throws Exception {
        final WebsitePermissionsWaiter waiter = new WebsitePermissionsWaiter();
        // Set lots of permissions values.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    for (String url : PERMISSION_URLS) {
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.GEOLOCATION,
                                        url,
                                        url,
                                        ContentSettingValues.BLOCK);
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.MIDI_SYSEX,
                                        url,
                                        url,
                                        ContentSettingValues.ALLOW);
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
                                        url,
                                        url,
                                        ContentSettingValues.BLOCK);
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.NOTIFICATIONS,
                                        url,
                                        url,
                                        ContentSettingValues.ALLOW);
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.MEDIASTREAM_MIC,
                                        url,
                                        url,
                                        ContentSettingValues.ALLOW);
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        profile,
                                        ContentSettingsType.MEDIASTREAM_CAMERA,
                                        url,
                                        url,
                                        ContentSettingValues.BLOCK);
                    }

                    // This should not time out. See crbug.com/732907.
                    WebsitePermissionsFetcher fetcher =
                            new WebsitePermissionsFetcher(mSiteSettingsDelegate);
                    fetcher.fetchAllPreferences(waiter);
                });
        waiter.waitForCallback(0, 1, 1000L, TimeUnit.MILLISECONDS);
    }

    static class FakeWebsitePreferenceBridge extends WebsitePreferenceBridge {
        public List<PermissionInfo> mPermissionInfos;
        public List<ContentSettingException> mContentSettingExceptions;
        public List<ChosenObjectInfo> mChosenObjectInfos;
        public HashMap<String, LocalStorageInfo> mLocalStorageInfoMap;
        public HashMap<String, LocalStorageInfo> mImportantLocalStorageInfoMap;
        public ArrayList<StorageInfo> mStorageInfos;
        public ArrayList<SharedDictionaryInfo> mSharedDictionaryInfos;

        FakeWebsitePreferenceBridge() {
            mPermissionInfos = new ArrayList<>();
            mContentSettingExceptions = new ArrayList<>();
            mChosenObjectInfos = new ArrayList<>();
            mLocalStorageInfoMap = new HashMap<>();
            mImportantLocalStorageInfoMap = new HashMap<>();
            mStorageInfos = new ArrayList<>();
            mSharedDictionaryInfos = new ArrayList<>();
        }

        @Override
        public List<PermissionInfo> getPermissionInfo(
                BrowserContextHandle browserContextHandle, @ContentSettingsType.EnumType int type) {
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
                @ContentSettingsType.EnumType int contentSettingsType) {
            List<ContentSettingException> result = new ArrayList<>();
            for (ContentSettingException exception : mContentSettingExceptions) {
                if (exception.getContentSettingType() == contentSettingsType) {
                    result.add(exception);
                }
            }
            return result;
        }

        @Override
        public void fetchLocalStorageInfo(
                BrowserContextHandle browserContextHandle,
                Callback<HashMap> callback,
                boolean fetchImportant) {
            if (fetchImportant) {
                callback.onResult(mImportantLocalStorageInfoMap);
                return;
            }
            callback.onResult(mLocalStorageInfoMap);
        }

        @Override
        public void fetchCookiesInfo(
                BrowserContextHandle browserContextHandle,
                Callback<Map<String, CookiesInfo>> callback) {
            callback.onResult(new HashMap<>());
        }

        @Override
        public void fetchStorageInfo(
                BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
            callback.onResult(mStorageInfos);
        }

        @Override
        public void fetchSharedDictionaryInfo(
                BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
            callback.onResult(mSharedDictionaryInfos);
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

        public void addSharedDictionaryInfo(SharedDictionaryInfo info) {
            mSharedDictionaryInfos.add(info);
        }

        public void addChosenObjectInfo(ChosenObjectInfo info) {
            mChosenObjectInfos.add(info);
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(BrowsingDataModelEnabled.class)
    public void testFetchAllPreferencesForSingleOrigin(boolean isBDMEnabled) {
        Mockito.doReturn(isBDMEnabled)
                .when(mSiteSettingsDelegate)
                .isBrowsingDataModelFeatureEnabled();
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.AR,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.HAND_TRACKING,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.IDLE_DETECTION,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.MIDI_SYSEX,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.NFC,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.NOTIFICATIONS,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.MEDIASTREAM_MIC,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.CLIPBOARD_READ_WRITE,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.SENSORS,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.VR,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));

        // Add content setting exception types.
        // If the ContentSettingsType.MAX_VALUE value changes *and* a new value has been exposed on
        // Android, then please update this code block to include a test for your new type.
        // Otherwise, just update count in the assert.
        // TODO(https://b/332704817): Add test for Tracking Protection content setting after Android
        // integration.
        assertEquals(114, ContentSettingsType.MAX_VALUE);
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.POPUPS,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.ADS,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.JAVASCRIPT,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.SOUND,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.BACKGROUND_SYNC,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.INSECURE_PRIVATE_NETWORK,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.JAVASCRIPT_JIT,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.REQUEST_DESKTOP_SITE,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.FEDERATED_IDENTITY_API,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.ANTI_ABUSE,
                        ORIGIN,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));

        int storageSize = 256;
        int sharedDictionarySize = 12345;
        if (isBDMEnabled) {
            var map = new HashMap<Origin, BrowsingDataInfo>();
            var origin = Origin.create(new GURL(ORIGIN));
            map.put(
                    origin,
                    new BrowsingDataInfo(origin, 0, storageSize + sharedDictionarySize, false));

            Mockito.when(
                            mBrowsingDataModel.getBrowsingDataInfo(
                                    mSiteSettingsDelegate.getBrowserContextHandle(), false))
                    .thenReturn(map);

            doAnswer(this::mockBDMCallback)
                    .when(mSiteSettingsDelegate)
                    .getBrowsingDataModel(any(Callback.class));
        } else {
            // Add storage info.
            websitePreferenceBridge.addStorageInfo(new StorageInfo(ORIGIN, 0, storageSize));

            // Add local storage info.
            websitePreferenceBridge.addLocalStorageInfoMapEntry(
                    new LocalStorageInfo(ORIGIN, storageSize, false));

            // Add shared dictionary info.
            websitePreferenceBridge.addSharedDictionaryInfo(
                    new SharedDictionaryInfo(ORIGIN, ORIGIN, sharedDictionarySize));
        }

        // Add chooser info types.
        websitePreferenceBridge.addChosenObjectInfo(
                new ChosenObjectInfo(
                        ContentSettingsType.USB_CHOOSER_DATA, ORIGIN, "Gadget", "Object", false));
        websitePreferenceBridge.addChosenObjectInfo(
                new ChosenObjectInfo(
                        ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
                        ORIGIN,
                        "Wireless",
                        "Object",
                        false));

        fetcher.fetchAllPreferences(
                (sites) -> {
                    assertEquals(1, sites.size());
                    Website site = sites.iterator().next();

                    Assert.assertTrue(site.getAddress().matches(ORIGIN));

                    // Check permission info types for |site|.
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
                    Assert.assertNotNull(
                            site.getPermissionInfo(ContentSettingsType.IDLE_DETECTION));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.MIDI_SYSEX));
                    Assert.assertNotNull(
                            site.getPermissionInfo(ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.NOTIFICATIONS));
                    Assert.assertNotNull(
                            site.getPermissionInfo(ContentSettingsType.MEDIASTREAM_CAMERA));
                    Assert.assertNotNull(
                            site.getPermissionInfo(ContentSettingsType.MEDIASTREAM_MIC));
                    Assert.assertNotNull(
                            site.getPermissionInfo(ContentSettingsType.CLIPBOARD_READ_WRITE));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.SENSORS));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.VR));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.HAND_TRACKING));
                    Assert.assertNotNull(site.getPermissionInfo(ContentSettingsType.AR));

                    // Check content setting exception types.
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.COOKIES));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.POPUPS));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.ADS));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.JAVASCRIPT));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.SOUND));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.BACKGROUND_SYNC));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.AUTOMATIC_DOWNLOADS));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.JAVASCRIPT_JIT));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.JAVASCRIPT_OPTIMIZER));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.AUTO_DARK_WEB_CONTENT));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.REQUEST_DESKTOP_SITE));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    ContentSettingsType.FEDERATED_IDENTITY_API));
                    assertEquals(
                            Integer.valueOf(ContentSettingValues.DEFAULT),
                            site.getContentSetting(
                                    UNUSED_BROWSER_CONTEXT_HANDLE, ContentSettingsType.ANTI_ABUSE));

                    if (isBDMEnabled) {
                        assertEquals(storageSize + sharedDictionarySize, site.getTotalUsage());
                    } else {
                        // Check storage info.
                        var storageInfos = new ArrayList<>(site.getStorageInfo());
                        assertEquals(1, storageInfos.size());

                        StorageInfo storageInfo = storageInfos.get(0);
                        assertEquals(ORIGIN, storageInfo.getHost());
                        assertEquals(storageSize, storageInfo.getSize());

                        // Check local storage info.
                        var localStorageInfo = site.getLocalStorageInfo();
                        assertEquals(ORIGIN, localStorageInfo.getOrigin());
                        assertEquals(storageSize, localStorageInfo.getSize());
                        Assert.assertFalse(localStorageInfo.isDomainImportant());

                        // Check shared dictionary info.
                        var sharedDictionaryInfos = new ArrayList<>(site.getSharedDictionaryInfo());
                        assertEquals(1, sharedDictionaryInfos.size());

                        SharedDictionaryInfo sharedDictionaryInfo = sharedDictionaryInfos.get(0);
                        assertEquals(ORIGIN, sharedDictionaryInfo.getOrigin());
                        assertEquals(sharedDictionarySize, sharedDictionaryInfo.getSize());
                    }

                    // Check chooser info types.
                    ArrayList<ChosenObjectInfo> chosenObjectInfos =
                            new ArrayList<>(site.getChosenObjectInfo());
                    assertEquals(2, chosenObjectInfos.size());
                    assertEquals(
                            ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
                            chosenObjectInfos.get(0).getContentSettingsType());
                    assertEquals(
                            ContentSettingsType.USB_CHOOSER_DATA,
                            chosenObjectInfos.get(1).getContentSettingsType());
                });
    }

    private Object mockBDMCallback(InvocationOnMock invocation) {
        var callback = (Callback<BrowsingDataModel>) invocation.getArguments()[0];
        callback.onResult(mBrowsingDataModel);
        return null;
    }

    @Test
    @SmallTest
    public void testFetchAllPreferencesForMultipleOrigins() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String chromiumOrigin = "https://chromium.org";
        String exampleOrigin = "https://example.com";

        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        chromiumOrigin,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));

        Website expectedGoogleWebsite =
                new Website(WebsiteAddress.create(ORIGIN), WebsiteAddress.create(null));
        Website expectedChromiumWebsite =
                new Website(WebsiteAddress.create(chromiumOrigin), WebsiteAddress.create(null));

        fetcher.fetchAllPreferences(
                (sites) -> {
                    assertEquals(2, sites.size());

                    // The order of |sites| is unknown, so check if the array contains a geolocation
                    // permission for each of the sites.
                    ArrayList<Website> siteArray = new ArrayList<>(sites);
                    boolean containsOriginPermission = false;
                    boolean containsChromiumOriginPermission = false;
                    for (Website site : siteArray) {
                        if (site.compareByAddressTo(expectedGoogleWebsite) == 0) {
                            containsOriginPermission = true;
                        } else if (site.compareByAddressTo(expectedChromiumWebsite) == 0) {
                            containsChromiumOriginPermission = true;
                        }

                        Assert.assertNotNull(
                                site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
                    }

                    Assert.assertTrue(containsOriginPermission);
                    Assert.assertTrue(containsChromiumOriginPermission);
                });

        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        exampleOrigin,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));

        Website expectedExampleWebsite =
                new Website(WebsiteAddress.create(exampleOrigin), WebsiteAddress.create(null));

        fetcher.fetchAllPreferences(
                (sites) -> {
                    assertEquals(3, sites.size());

                    ArrayList<Website> siteArray = new ArrayList<>(sites);
                    boolean containsOriginPermission = false;
                    boolean containsChromiumOriginPermission = false;
                    boolean containsExampleOriginPermission = false;
                    for (Website site : siteArray) {
                        if (site.compareByAddressTo(expectedGoogleWebsite) == 0) {
                            containsOriginPermission = true;
                        } else if (site.compareByAddressTo(expectedChromiumWebsite) == 0) {
                            containsChromiumOriginPermission = true;
                        } else if (site.compareByAddressTo(expectedExampleWebsite) == 0) {
                            containsExampleOriginPermission = true;
                        }

                        Assert.assertNotNull(
                                site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
                    }

                    Assert.assertTrue(containsOriginPermission);
                    Assert.assertTrue(containsChromiumOriginPermission);
                    Assert.assertTrue(containsExampleOriginPermission);
                });
    }

    public void assertContentSettingExceptionEquals(
            ContentSettingException expected, ContentSettingException actual) {
        assertEquals(expected.getSource(), actual.getSource());
        assertEquals(expected.isEmbargoed(), actual.isEmbargoed());
        assertEquals(expected.getPrimaryPattern(), actual.getPrimaryPattern());
        assertEquals(expected.getSecondaryPattern(), actual.getSecondaryPattern());
        assertEquals(expected.getContentSetting(), actual.getContentSetting());
        assertEquals(expected.getExpirationInDays(), actual.getExpirationInDays());
        assertEquals(expected.hasExpiration(), actual.hasExpiration());
    }

    @Test
    @SmallTest
    @UseMethodParameter(EmbargoedAndOneTimeSessionParameters.class)
    public void testFetchPreferencesForCategoryPermissionInfoTypes(
            boolean isEmbargoed, boolean isOneTime) {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        // MIDI is excluded from this list because it does not have a top level category.
        ArrayList<Integer> permissionInfoTypes =
                new ArrayList<>(
                        Arrays.asList(
                                ContentSettingsType.AR,
                                ContentSettingsType.MEDIASTREAM_CAMERA,
                                ContentSettingsType.CLIPBOARD_READ_WRITE,
                                ContentSettingsType.GEOLOCATION,
                                ContentSettingsType.HAND_TRACKING,
                                ContentSettingsType.IDLE_DETECTION,
                                ContentSettingsType.MEDIASTREAM_MIC,
                                ContentSettingsType.NFC,
                                ContentSettingsType.NOTIFICATIONS,
                                ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
                                ContentSettingsType.SENSORS,
                                ContentSettingsType.VR));

        @SessionModel.EnumType
        int sessionModel = isOneTime ? SessionModel.ONE_TIME : SessionModel.DURABLE;
        for (@ContentSettingsType.EnumType int type : permissionInfoTypes) {
            PermissionInfo fakePermissionInfo =
                    new PermissionInfo(type, ORIGIN, SITE_WILDCARD, isEmbargoed, sessionModel);
            websitePreferenceBridge.addPermissionInfo(fakePermissionInfo);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromContentSettingsType(
                            UNUSED_BROWSER_CONTEXT_HANDLE, type),
                    (sites) -> {
                        assertEquals(1, sites.size());

                        Website site = sites.iterator().next();
                        Assert.assertNotNull(site.getPermissionInfo(type));
                        Assert.assertEquals(
                                sessionModel, site.getPermissionInfo(type).getSessionModel());
                    });
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(EmbargoedParams.class)
    public void testFetchPreferencesForCategoryContentSettingExceptionTypes(boolean isEmbargoed) {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        ArrayList<Integer> contentSettingExceptionTypes =
                new ArrayList<>(
                        Arrays.asList(
                                ContentSettingsType.ADS,
                                ContentSettingsType.AUTOMATIC_DOWNLOADS,
                                ContentSettingsType.BACKGROUND_SYNC,
                                ContentSettingsType.BLUETOOTH_SCANNING,
                                ContentSettingsType.COOKIES,
                                ContentSettingsType.FEDERATED_IDENTITY_API,
                                ContentSettingsType.JAVASCRIPT,
                                ContentSettingsType.POPUPS,
                                ContentSettingsType.SOUND));

        for (@ContentSettingsType.EnumType int type : contentSettingExceptionTypes) {
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(
                                type,
                                ORIGIN,
                                ContentSettingValues.DEFAULT,
                                ProviderType.PREF_PROVIDER,
                                isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, type),
                        (sites) -> {
                            assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(
                                    fakeContentSettingException,
                                    site.getContentSettingException(type));
                        });
            }

            // Make sure that the content setting value is updated.
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(
                                type,
                                ORIGIN,
                                ContentSettingValues.BLOCK,
                                ProviderType.PREF_PROVIDER,
                                isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, type),
                        (sites) -> {
                            assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(
                                    fakeContentSettingException,
                                    site.getContentSettingException(type));
                        });
            }
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(EmbargoedParams.class)
    public void testFetchPreferencesForAdvancedCookieSettings(boolean isEmbargoed) {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String mainSite = "https://a.com";
        String thirdPartySite = "https://b.com";
        @ContentSettingsType.EnumType int contentSettingsType = ContentSettingsType.COOKIES;

        // Test the advanced exception combinations of:
        // b.com on a.com
        // a.com on a.com
        // * on a.com
        ArrayList<Pair<String, String>> exceptions =
                new ArrayList<>(
                        Arrays.asList(
                                new Pair<>(thirdPartySite, mainSite),
                                new Pair<>(mainSite, mainSite),
                                new Pair<>(SITE_WILDCARD, mainSite)));

        for (Pair<String, String> pair : exceptions) {
            websitePreferenceBridge.resetContentSettingExceptions();
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(
                                contentSettingsType,
                                pair.first,
                                pair.second,
                                ContentSettingValues.DEFAULT,
                                ProviderType.PREF_PROVIDER,
                                EXPIRATION_IN_DAYS,
                                isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, contentSettingsType),
                        (sites) -> {
                            assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(
                                    fakeContentSettingException,
                                    site.getContentSettingException(contentSettingsType));
                        });
            }

            // Make sure that the content setting value is updated.
            {
                ContentSettingException fakeContentSettingException =
                        new ContentSettingException(
                                contentSettingsType,
                                pair.first,
                                pair.second,
                                ContentSettingValues.BLOCK,
                                ProviderType.PREF_PROVIDER,
                                EXPIRATION_IN_DAYS,
                                isEmbargoed);
                websitePreferenceBridge.addContentSettingException(fakeContentSettingException);

                fetcher.fetchPreferencesForCategory(
                        SiteSettingsCategory.createFromContentSettingsType(
                                UNUSED_BROWSER_CONTEXT_HANDLE, contentSettingsType),
                        (sites) -> {
                            assertEquals(1, sites.size());

                            Website site = sites.iterator().next();
                            assertContentSettingExceptionEquals(
                                    fakeContentSettingException,
                                    site.getContentSettingException(contentSettingsType));
                        });
            }
        }
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryStorageInfo() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String chromiumOrigin = "https://chromium.org";
        int storageSize = 256;
        int sharedDictionarySize = 512;
        StorageInfo fakeStorageInfo = new StorageInfo(ORIGIN, 0, storageSize);
        LocalStorageInfo fakeLocalStorageInfo = new LocalStorageInfo(ORIGIN, storageSize, false);
        LocalStorageInfo fakeImportantLocalStorageInfo =
                new LocalStorageInfo(chromiumOrigin, storageSize, true);
        SharedDictionaryInfo fakeSharedDictionaryInfo =
                new SharedDictionaryInfo(ORIGIN, ORIGIN, sharedDictionarySize);

        websitePreferenceBridge.addStorageInfo(fakeStorageInfo);
        websitePreferenceBridge.addLocalStorageInfoMapEntry(fakeLocalStorageInfo);
        websitePreferenceBridge.addLocalStorageInfoMapEntry(fakeImportantLocalStorageInfo);
        websitePreferenceBridge.addSharedDictionaryInfo(fakeSharedDictionaryInfo);

        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(
                        UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.USE_STORAGE),
                (sites) -> {
                    assertEquals(1, sites.size());

                    Website site = sites.iterator().next();
                    List<StorageInfo> storageInfos = site.getStorageInfo();
                    assertEquals(1, storageInfos.size());

                    StorageInfo storageInfo = storageInfos.get(0);
                    assertEquals(fakeStorageInfo.getSize(), storageInfo.getSize());
                    assertEquals(fakeStorageInfo.getHost(), storageInfo.getHost());

                    LocalStorageInfo localStorageInfo = site.getLocalStorageInfo();
                    Assert.assertFalse(localStorageInfo.isDomainImportant());
                    assertEquals(fakeLocalStorageInfo.getSize(), localStorageInfo.getSize());
                    assertEquals(fakeLocalStorageInfo.getOrigin(), localStorageInfo.getOrigin());

                    List<SharedDictionaryInfo> sharedDictionaryInfos =
                            site.getSharedDictionaryInfo();
                    assertEquals(1, sharedDictionaryInfos.size());

                    SharedDictionaryInfo sharedDictionaryInfo = sharedDictionaryInfos.get(0);
                    assertEquals(
                            fakeSharedDictionaryInfo.getOrigin(), sharedDictionaryInfo.getOrigin());
                    assertEquals(
                            fakeSharedDictionaryInfo.getSize(), sharedDictionaryInfo.getSize());
                });

        // Test that the fetcher gets local storage info for important domains.
        fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate, true);
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(
                        UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.USE_STORAGE),
                (sites) -> {
                    assertEquals(2, sites.size());

                    for (Website site : sites) {
                        if (site.getAddress().matches(ORIGIN)) {
                            List<StorageInfo> storageInfos = site.getStorageInfo();
                            assertEquals(1, storageInfos.size());

                            StorageInfo storageInfo = storageInfos.get(0);
                            assertEquals(fakeStorageInfo.getSize(), storageInfo.getSize());
                            assertEquals(fakeStorageInfo.getHost(), storageInfo.getHost());

                            Assert.assertNull(site.getLocalStorageInfo());

                            List<SharedDictionaryInfo> sharedDictionaryInfos =
                                    site.getSharedDictionaryInfo();
                            assertEquals(1, sharedDictionaryInfos.size());

                            SharedDictionaryInfo sharedDictionaryInfo =
                                    sharedDictionaryInfos.get(0);
                            assertEquals(
                                    fakeSharedDictionaryInfo.getOrigin(),
                                    sharedDictionaryInfo.getOrigin());
                            assertEquals(
                                    fakeSharedDictionaryInfo.getSize(),
                                    sharedDictionaryInfo.getSize());
                        } else if (site.getAddress().matches(chromiumOrigin)) {
                            List<StorageInfo> storageInfos = site.getStorageInfo();
                            assertEquals(0, storageInfos.size());

                            LocalStorageInfo localStorageInfo = site.getLocalStorageInfo();
                            Assert.assertTrue(localStorageInfo.isDomainImportant());
                            assertEquals(
                                    fakeImportantLocalStorageInfo.getSize(),
                                    localStorageInfo.getSize());
                            assertEquals(
                                    fakeImportantLocalStorageInfo.getOrigin(),
                                    localStorageInfo.getOrigin());

                            List<SharedDictionaryInfo> sharedDictionaryInfos =
                                    site.getSharedDictionaryInfo();
                            assertEquals(0, sharedDictionaryInfos.size());
                        } else {
                            Assert.fail(
                                    "The WebsitePermissionsFetcher should only return "
                                            + "Website objects for the granted origins.");
                        }
                    }
                });
    }

    @Test
    @SmallTest
    public void testFetchPreferencesForCategoryChooserDataTypes() {
        ArrayList<Integer> chooserDataTypes =
                new ArrayList<>(
                        Arrays.asList(
                                SiteSettingsCategory.Type.USB,
                                SiteSettingsCategory.Type.BLUETOOTH));

        for (@SiteSettingsCategory.Type int type : chooserDataTypes) {
            WebsitePermissionsFetcher fetcher =
                    new WebsitePermissionsFetcher(mSiteSettingsDelegate);
            FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
            fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
            @ContentSettingsType.EnumType
            int chooserDataType =
                    SiteSettingsCategory.objectChooserDataTypeFromGuard(
                            SiteSettingsCategory.contentSettingsType(type));
            Assert.assertNotEquals(-1, chooserDataType);

            ChosenObjectInfo fakeObjectInfo =
                    new ChosenObjectInfo(
                            chooserDataType,
                            ORIGIN,
                            "Chosen Object",
                            "SerializedObjectData",
                            false);
            websitePreferenceBridge.addChosenObjectInfo(fakeObjectInfo);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromType(UNUSED_BROWSER_CONTEXT_HANDLE, type),
                    (sites) -> {
                        assertEquals(1, sites.size());

                        List<ChosenObjectInfo> objectInfos =
                                new ArrayList<>(sites.iterator().next().getChosenObjectInfo());
                        assertEquals(1, objectInfos.size());
                        assertEquals(fakeObjectInfo, objectInfos.get(0));
                    });
        }
    }

    @Test
    @SmallTest
    public void testGetRelatedWebsiteSetsOwnersAndMergeInfoIntoWebsites() {
        for (var entry : RWS_MEMBER_TO_OWNER_MAP.entrySet()) {
            Mockito.doReturn(entry.getValue())
                    .when(mSiteSettingsDelegate)
                    .getRelatedWebsiteSetOwner(entry.getKey());
        }

        Mockito.doReturn(true).when(mSiteSettingsDelegate).isRelatedWebsiteSetsDataAccessEnabled();
        Mockito.doReturn(true)
                .when(mSiteSettingsDelegate)
                .isPrivacySandboxFirstPartySetsUIFeatureEnabled();

        var fetcher =
                new WebsitePermissionsFetcher(
                        mSiteSettingsDelegate, /* fetchSiteImportantInfo= */ false);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String googleDEOrigin = "https://google.de";
        String googleITOrigin = "https://google.it";
        String googleCHOrigin = "https://google.ch";
        String youtubeOrigin = "https://youtube.com";
        String verizonConnectOrigin = "https://verizonconnect.com";

        String aolOrigin = "https://aol.com";
        String noInRWSOrigin = "https://unknow.ch";

        Website expectedYoutubeWebsite =
                new Website(WebsiteAddress.create(youtubeOrigin), WebsiteAddress.create(null));
        Website expectedVerizonConnectWebsite =
                new Website(
                        WebsiteAddress.create(verizonConnectOrigin), WebsiteAddress.create(null));
        Website expectedNoInRWSWebsite =
                new Website(WebsiteAddress.create(noInRWSOrigin), WebsiteAddress.create(null));

        // Use a list of origins and create content settings exceptions.
        List<String> origins =
                Arrays.asList(
                        googleDEOrigin,
                        googleITOrigin,
                        googleCHOrigin,
                        youtubeOrigin,
                        verizonConnectOrigin,
                        aolOrigin,
                        noInRWSOrigin);
        // Adding content exceptions will generate websites data.
        for (String origin : origins) {
            websitePreferenceBridge.addContentSettingException(
                    new ContentSettingException(
                            ContentSettingsType.COOKIES,
                            origin,
                            ContentSettingValues.ALLOW,
                            ProviderType.PREF_PROVIDER,
                            /* isEmbargoed= */ false));
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fetcher.fetchPreferencesForCategoryAndPopulateRwsInfo(
                            SiteSettingsCategory.createFromType(
                                    UNUSED_BROWSER_CONTEXT_HANDLE,
                                    SiteSettingsCategory.Type.ALL_SITES),
                            (sites) -> {
                                // Verify the number of sites is the same of the origins with
                                // exceptions.
                                assertEquals(origins.size(), sites.size());

                                ArrayList<Website> siteArray = new ArrayList<>(sites);
                                for (Website site : siteArray) {
                                    // Verify youtube.com has google.com as RWS owner which has 4
                                    // members within the group of sites with data.
                                    if (site.compareByAddressTo(expectedYoutubeWebsite) == 0) {
                                        Assert.assertNotNull(site.getRWSCookieInfo());
                                        assertEquals(
                                                "google.com", site.getRWSCookieInfo().getOwner());
                                        assertEquals(4, site.getRWSCookieInfo().getMembersCount());
                                    }
                                    // Verify verizonconnect.com has verizon.com as RWS owner which
                                    // has 2 members within the group of sites with data.
                                    if (site.compareByAddressTo(expectedVerizonConnectWebsite)
                                            == 0) {
                                        Assert.assertNotNull(site.getRWSCookieInfo());
                                        assertEquals(
                                                "verizon.com", site.getRWSCookieInfo().getOwner());
                                        assertEquals(2, site.getRWSCookieInfo().getMembersCount());
                                    }

                                    // Verify a website with data which is not in a RWS has no RWS
                                    // data set.
                                    if (site.compareByAddressTo(expectedNoInRWSWebsite) == 0) {
                                        assertEquals(null, site.getRWSCookieInfo());
                                    }
                                }
                            });
                });
    }

    @Test
    @SmallTest
    public void testIncognitoFetching() throws TimeoutException {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);
        String origin = "https://example.com";
        final WebsitePermissionsWaiter waiter = new WebsitePermissionsWaiter();

        // Add a ALLOW exception and a ASK exception for the same pattern to simulate a permission
        // from regular mode that was inherited as ASK and a permission from incognito mode.
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        origin,
                        origin,
                        ContentSettingValues.ALLOW,
                        ProviderType.NONE,
                        null,
                        false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        origin,
                        origin,
                        ContentSettingValues.ASK,
                        ProviderType.NONE,
                        null,
                        false));
        fetcher.fetchAllPreferences(waiter);
        waiter.waitForOnly();

        // Check that only the ALLOW exception is fetched.
        assertEquals(1, waiter.getSites().size());
        var site = waiter.getSites().iterator().next();
        var permission = site.getEmbeddedPermissions().get(ContentSettingsType.STORAGE_ACCESS);
        assertEquals(1, permission.size());
        assertEquals(ContentSettingValues.ALLOW, (int) permission.get(0).getContentSetting());
    }

    @Test
    @SmallTest
    public void testFetchAllSites() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        ORIGIN,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addPermissionInfo(
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        EMBEDDER,
                        SITE_WILDCARD,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        ORIGIN,
                        EMBEDDER,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        EXPIRATION_IN_DAYS,
                        /* isEmbargoed= */ false));
        websitePreferenceBridge.addContentSettingException(
                new ContentSettingException(
                        ContentSettingsType.STORAGE_ACCESS,
                        ORIGIN,
                        null,
                        ContentSettingValues.DEFAULT,
                        ProviderType.PREF_PROVIDER,
                        EXPIRATION_IN_DAYS,
                        /* isEmbargoed= */ true));

        Website expectedWebsite =
                new Website(WebsiteAddress.create(ORIGIN), WebsiteAddress.create(null));
        Website expectedEmbedderWebsite =
                new Website(WebsiteAddress.create(EMBEDDER), WebsiteAddress.create(null));

        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(
                        UNUSED_BROWSER_CONTEXT_HANDLE, SiteSettingsCategory.Type.ALL_SITES),
                (sites) -> {
                    Assert.assertEquals(2, sites.size());

                    // The order of |sites| is unknown, so check if the array contains a geolocation
                    // permission for each of the sites.
                    ArrayList<Website> siteArray = new ArrayList<>(sites);
                    boolean containsOriginPermission = false;
                    boolean containsEmbedderOriginPermission = false;
                    for (Website site : siteArray) {
                        if (site.compareByAddressTo(expectedWebsite) == 0) {
                            containsOriginPermission = true;

                            // Check that embargoed Storage Access is grouped by the origin.
                            Assert.assertEquals(
                                    Integer.valueOf(ContentSettingValues.DEFAULT),
                                    site.getContentSetting(
                                            UNUSED_BROWSER_CONTEXT_HANDLE,
                                            ContentSettingsType.STORAGE_ACCESS));
                            Assert.assertTrue(
                                    site.getEmbeddedPermissions()
                                            .get(ContentSettingsType.STORAGE_ACCESS)
                                            .get(0)
                                            .isEmbargoed());

                        } else if (site.compareByAddressTo(expectedEmbedderWebsite) == 0) {
                            containsEmbedderOriginPermission = true;

                            // Check that a normal Storage Access is grouped by the embedder.
                            Assert.assertEquals(
                                    Integer.valueOf(ContentSettingValues.DEFAULT),
                                    site.getContentSetting(
                                            UNUSED_BROWSER_CONTEXT_HANDLE,
                                            ContentSettingsType.STORAGE_ACCESS));
                            Assert.assertFalse(
                                    site.getEmbeddedPermissions()
                                            .get(ContentSettingsType.STORAGE_ACCESS)
                                            .get(0)
                                            .isEmbargoed());
                        }

                        Assert.assertNotNull(
                                site.getPermissionInfo(ContentSettingsType.GEOLOCATION));
                    }

                    Assert.assertTrue(containsOriginPermission);
                    Assert.assertTrue(containsEmbedderOriginPermission);
                });
    }

    @Test
    @SmallTest
    @UseMethodParameter(EmbargoedParams.class)
    public void testFetchPreferencesForCategoryEmbeddedPermissionTypes(boolean isEmbargoed) {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(mSiteSettingsDelegate);
        FakeWebsitePreferenceBridge websitePreferenceBridge = new FakeWebsitePreferenceBridge();
        fetcher.setWebsitePreferenceBridgeForTesting(websitePreferenceBridge);

        String embedder = isEmbargoed ? null : EMBEDDER;

        for (@ContentSettingsType.EnumType int type : EMBEDDED_CONTENT_SETTINGS) {
            ContentSettingException fakeContentSetting =
                    new ContentSettingException(
                            type,
                            ORIGIN,
                            embedder,
                            ContentSettingValues.DEFAULT,
                            ProviderType.PREF_PROVIDER,
                            EXPIRATION_IN_DAYS,
                            isEmbargoed);
            websitePreferenceBridge.addContentSettingException(fakeContentSetting);

            fetcher.fetchPreferencesForCategory(
                    SiteSettingsCategory.createFromContentSettingsType(
                            UNUSED_BROWSER_CONTEXT_HANDLE, type),
                    (sites) -> {
                        Assert.assertEquals(1, sites.size());

                        Website site = sites.iterator().next();
                        List<ContentSettingException> exceptions =
                                site.getEmbeddedPermissions().get(type);
                        Assert.assertEquals(1, exceptions.size());
                        assertContentSettingExceptionEquals(fakeContentSetting, exceptions.get(0));
                    });
        }
    }
}
