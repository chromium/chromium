// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.mockito.Mockito.doReturn;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.widget.ListView;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.favicon.GoogleFaviconServerRequestStatus;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.components.search_engines.PrepopulatedAndRecentlyVisitedTemplateURLs;
import org.chromium.components.search_engines.SearchEngineSettingsDataProvider;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for Search Engine Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SearchEngineSettingsRenderTest {
    private static final int RENDER_TEST_REVISION = 2;
    public final @Rule BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public final @Rule ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .setRevision(RENDER_TEST_REVISION)
                    .build();

    public final @Rule MockitoRule mMocks = MockitoJUnit.rule();

    private @Mock RegionalCapabilitiesService mMockRegionalCapabilities;
    private @Mock TemplateUrlService mMockTemplateUrlService;
    private @Mock SearchEngineSettingsDataProvider mMockSettingsDataProvider;
    private @Mock Profile mProfile;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNativeMock;

    @Before
    public void setUp() {
        doReturn(mMockSettingsDataProvider)
                .when(mMockTemplateUrlService)
                .createSettingsDataProvider();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    public void testRenderWithSecFeature_Legacy() throws Exception {
        TemplateUrl engine1 = buildTemplateUrl("Custom Engine", 0);
        GURL engine1Gurl = new GURL("https://gurl1.example.com");
        TemplateUrl engine2 = buildTemplateUrl("Prepopulated Engine", 2);
        GURL engine2Gurl = new GURL("https://gurl2.example.com");
        List<TemplateUrl> templateUrls = List.of(engine1, engine2);

        doReturn(true).when(mMockRegionalCapabilities).isInEeaCountry();
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mMockRegionalCapabilities);

        doReturn(new ArrayList<>(templateUrls)).when(mMockTemplateUrlService).getTemplateUrls();
        doReturn(engine1).when(mMockTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(true).when(mMockTemplateUrlService).isLoaded();
        String engine1Keyword = engine1.getKeyword();
        doReturn(engine1Gurl.getSpec())
                .when(mMockTemplateUrlService)
                .getSearchEngineUrlFromTemplateUrl(engine1Keyword);
        String engine2Keyword = engine2.getKeyword();
        doReturn(engine2Gurl.getSpec())
                .when(mMockTemplateUrlService)
                .getSearchEngineUrlFromTemplateUrl(engine2Keyword);

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNativeMock);

        mActivityTestRule.launchActivity(null);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge(mProfile);

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FragmentManager fragmentManager =
                                    mActivityTestRule.getActivity().getSupportFragmentManager();
                            SearchEngineSettings fragment =
                                    (SearchEngineSettings)
                                            fragmentManager
                                                    .getFragmentFactory()
                                                    .instantiate(
                                                            SearchEngineSettings.class
                                                                    .getClassLoader(),
                                                            SearchEngineSettings.class.getName());
                            fragment.setProfile(mProfile);

                            SearchEngineAdapter adapter =
                                    new SearchEngineAdapter(
                                            mActivityTestRule.getActivity(),
                                            mProfile,
                                            /* siteSearchClickHandler= */ null) {
                                        @Override
                                        LargeIconBridge createLargeIconBridge() {
                                            return largeIconBridge;
                                        }
                                    };
                            fragment.overrideSearchEngineAdapterForTesting(adapter);

                            fragmentManager
                                    .beginTransaction()
                                    .replace(android.R.id.content, fragment)
                                    .commitNow();

                            return fragment.getView();
                        });

        // Wait for icons to be requested.
        CriteriaHelper.pollUiThread(() -> largeIconBridge.getCallbackCount() == 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Bitmap bitmap1 = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    bitmap1.eraseColor(Color.GREEN);
                    largeIconBridge.provideFaviconForUrl(engine1Gurl, bitmap1);

                    Bitmap bitmap2 = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    bitmap2.eraseColor(Color.BLUE);
                    largeIconBridge.provideFaviconForUrl(engine2Gurl, bitmap2);
                });
        mRenderTestRule.render(view, "search_engine_settings_legacy");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testRenderWithSecFeature_New() throws Exception {
        TemplateUrl engine1 = buildTemplateUrl("Custom Default Engine", 0);
        GURL engine1Gurl = new GURL("https://gurl1.example.com");
        TemplateUrl engine2 = buildTemplateUrl("Prepopulated Engine", 2);
        GURL engine2Gurl = new GURL("https://gurl2.example.com");
        TemplateUrl engine3 = buildTemplateUrl("Recently Visited Engine", 0);
        GURL engine3Gurl = new GURL("https://gurl3.example.com");

        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mMockRegionalCapabilities);

        // engine1 is the default search engine, so it also gets added to the first list.
        // engine2 (prepopulated) should come before engine1 (custom DSE) in the list.
        doReturn(
                        new PrepopulatedAndRecentlyVisitedTemplateURLs(
                                List.of(engine2, engine1), List.of(engine3)))
                .when(mMockSettingsDataProvider)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();
        doReturn(engine1).when(mMockTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(true).when(mMockTemplateUrlService).isLoaded();
        String engine1Keyword = engine1.getKeyword();
        doReturn(engine1Gurl.getSpec())
                .when(mMockTemplateUrlService)
                .getSearchEngineUrlFromTemplateUrl(engine1Keyword);
        String engine2Keyword = engine2.getKeyword();
        doReturn(engine2Gurl.getSpec())
                .when(mMockTemplateUrlService)
                .getSearchEngineUrlFromTemplateUrl(engine2Keyword);
        String engine3Keyword = engine3.getKeyword();
        doReturn(engine3Gurl.getSpec())
                .when(mMockTemplateUrlService)
                .getSearchEngineUrlFromTemplateUrl(engine3Keyword);

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNativeMock);

        mActivityTestRule.launchActivity(null);
        TestLargeIconBridge largeIconBridge = new TestLargeIconBridge(mProfile);

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FragmentManager fragmentManager =
                                    mActivityTestRule.getActivity().getSupportFragmentManager();
                            SearchEngineSettings fragment =
                                    (SearchEngineSettings)
                                            fragmentManager
                                                    .getFragmentFactory()
                                                    .instantiate(
                                                            SearchEngineSettings.class
                                                                    .getClassLoader(),
                                                            SearchEngineSettings.class.getName());
                            fragment.setProfile(mProfile);

                            SearchEngineAdapter adapter =
                                    new SearchEngineAdapter(
                                            mActivityTestRule.getActivity(),
                                            mProfile,
                                            /* siteSearchClickHandler= */ null) {
                                        @Override
                                        LargeIconBridge createLargeIconBridge() {
                                            return largeIconBridge;
                                        }
                                    };
                            fragment.overrideSearchEngineAdapterForTesting(adapter);

                            fragmentManager
                                    .beginTransaction()
                                    .replace(android.R.id.content, fragment)
                                    .commitNow();

                            return fragment.getView();
                        });

        // Scroll to the end of the list to ensure all items are bound and icons requested.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchEngineSettings fragment =
                            (SearchEngineSettings)
                                    mActivityTestRule
                                            .getActivity()
                                            .getSupportFragmentManager()
                                            .findFragmentById(android.R.id.content);
                    ListView listView = fragment.getListView();
                    listView.smoothScrollToPosition(listView.getAdapter().getCount() - 1);
                });

        // Wait for icons to be requested.
        CriteriaHelper.pollUiThread(() -> largeIconBridge.getCallbackCount() == 3);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Bitmap bitmap1 = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    bitmap1.eraseColor(Color.GREEN);
                    largeIconBridge.provideFaviconForUrl(engine1Gurl, bitmap1);

                    Bitmap bitmap2 = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    bitmap2.eraseColor(Color.BLUE);
                    largeIconBridge.provideFaviconForUrl(engine2Gurl, bitmap2);

                    Bitmap bitmap3 = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888);
                    bitmap3.eraseColor(Color.RED);
                    largeIconBridge.provideFaviconForUrl(engine3Gurl, bitmap3);
                });
        mRenderTestRule.render(view, "search_engine_settings_new");
    }

    private static TemplateUrl buildTemplateUrl(String shortName, int prepopulatedId) {
        TemplateUrl templateUrl = buildMockTemplateUrl("keyword_" + shortName, prepopulatedId);
        doReturn(shortName).when(templateUrl).getShortName();
        return templateUrl;
    }

    private static class TestLargeIconBridge extends LargeIconBridge {
        private final Map<GURL, LargeIconCallback> mCallbacks = new HashMap<>();

        TestLargeIconBridge(BrowserContextHandle browserContextHandle) {
            super(browserContextHandle);
        }

        @Override
        public boolean getLargeIconForUrl(
                final GURL pageUrl,
                int minSizePx,
                int desiredSizePx,
                final LargeIconCallback callback) {
            mCallbacks.put(pageUrl, callback);
            return true;
        }

        public void provideFaviconForUrl(GURL pageUrl, Bitmap bitmap) {
            LargeIconCallback callback = mCallbacks.get(pageUrl);
            callback.onLargeIconAvailable(bitmap, Color.BLACK, false, IconType.INVALID);
            mCallbacks.remove(pageUrl);
        }

        public int getCallbackCount() {
            return mCallbacks.size();
        }

        @Override
        public void getLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                GURL pageUrl,
                boolean shouldTrimPageUrlPath,
                NetworkTrafficAnnotationTag trafficAnnotation,
                GoogleFaviconServerCallback callback) {
            callback.onRequestComplete(GoogleFaviconServerRequestStatus.SUCCESS);
        }

        @Override
        public void touchIconFromGoogleServer(GURL iconUrl) {}
    }
}
