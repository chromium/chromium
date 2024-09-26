// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.mockito.Mockito.doReturn;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.favicon.GoogleFaviconServerRequestStatus;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
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
    public final @Rule BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public final @Rule ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .setRevision(1)
                    .build();

    public final @Rule MockitoRule mMocks = MockitoJUnit.rule();

    public final @Rule JniMocker mJniMocker = new JniMocker();

    private @Mock TemplateUrlService mMockTemplateUrlService;
    private @Mock Profile mProfile;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNativeMock;

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithSecFeature() throws Exception {
        TemplateUrl engine1 = buildTemplateUrl("Custom Engine", 0);
        GURL engine1Gurl = new GURL("https://gurl1.example.com");
        TemplateUrl engine2 = buildTemplateUrl("Prepopulated Engine", 2);
        GURL engine2Gurl = new GURL("https://gurl2.example.com");
        List<TemplateUrl> templateUrls = List.of(engine1, engine2);

        doReturn(new ArrayList<>(templateUrls)).when(mMockTemplateUrlService).getTemplateUrls();
        doReturn(engine1).when(mMockTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(true).when(mMockTemplateUrlService).isEeaChoiceCountry();
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
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeNativeMock);

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
                                            mActivityTestRule.getActivity(), mProfile) {
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
        mRenderTestRule.render(view, "search_engine_settings");
    }

    private static TemplateUrl buildTemplateUrl(String shortName, int prepopulatedId) {
        TemplateUrl templateUrl =
                buildMockTemplateUrl("prepopulatedId=" + prepopulatedId, prepopulatedId);
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
