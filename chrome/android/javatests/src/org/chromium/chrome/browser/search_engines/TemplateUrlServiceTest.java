// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.not;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineAdapter;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.stream.Collectors;

/** Tests for Chrome on Android's usage of the TemplateUrlService API. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TemplateUrlServiceTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private static final String QUERY_PARAMETER = "q";
    private static final String QUERY_VALUE = "cat";

    private static final String ALTERNATIVE_PARAMETER = "ctxsl_alternate_term";
    private static final String ALTERNATIVE_VALUE = "lion";

    private static final String VERSION_PARAMETER = "ctxs";
    private static final String VERSION_VALUE_TWO_REQUEST_PROTOCOL = "2";
    private static final String VERSION_VALUE_SINGLE_REQUEST_PROTOCOL = "3";

    private static final String PREFETCH_PARAMETER = "pf";
    private static final String PREFETCH_VALUE = "c";

    private static final String PLAY_API_SEARCH_URL = "https://play.search.engine?q={searchTerms}";
    private static final String PLAY_API_SUGGEST_URL = "https://suggest.engine?q={searchTerms}";
    private static final String PLAY_API_FAVICON_URL = "https://fav.icon";
    private static final String PLAY_API_NEW_TAB_URL = "https://search.engine/newtab";
    private static final String PLAY_API_IMAGE_URL = "https://search.engine/img";
    private static final String PLAY_API_IMAGE_POST_PARAM = "img";
    private static final String PLAY_API_IMAGE_TRANSLATE_URL = "https://search.engine/imgtransl";
    private static final String PLAY_API_IMAGE_TRANSLATE_SOURCE_KEY = "source";
    private static final String PLAY_API_IMAGE_TRANSLATE_DEST_KEY = "dest";

    private TemplateUrlService mTemplateUrlService;

    @Before
    public void setUp() {
        mTemplateUrlService =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testUrlForContextualSearchQueryValid() throws ExecutionException {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return mTemplateUrlService.isLoaded();
                            }
                        }));

        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_SINGLE_REQUEST_PROTOCOL);
    }

    private void validateQuery(
            final String query,
            final String alternative,
            final boolean prefetch,
            final String protocolVersion)
            throws ExecutionException {
        GURL result =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<GURL>() {
                            @Override
                            public GURL call() {
                                return mTemplateUrlService.getUrlForContextualSearchQuery(
                                        query, alternative, prefetch, protocolVersion);
                            }
                        });
        Assert.assertNotNull(result);
        Uri uri = Uri.parse(result.getSpec());
        Assert.assertEquals(query, uri.getQueryParameter(QUERY_PARAMETER));
        Assert.assertEquals(alternative, uri.getQueryParameter(ALTERNATIVE_PARAMETER));
        Assert.assertEquals(protocolVersion, uri.getQueryParameter(VERSION_PARAMETER));
        if (prefetch) {
            Assert.assertEquals(PREFETCH_VALUE, uri.getQueryParameter(PREFETCH_PARAMETER));
        } else {
            Assert.assertNull(uri.getQueryParameter(PREFETCH_PARAMETER));
        }
    }

    private void validateSearchQuery(
            final String query,
            final List<String> searchParams,
            final Map<String, String> expectedParams)
            throws ExecutionException {
        String result =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<String>() {
                            @Override
                            public String call() {
                                return mTemplateUrlService.getUrlForSearchQuery(
                                        query, searchParams);
                            }
                        });
        Assert.assertNotNull(result);
        Uri uri = Uri.parse(result);
        Assert.assertEquals(query, uri.getQueryParameter(QUERY_PARAMETER));
        if (expectedParams == null) return;
        for (Map.Entry<String, String> param : expectedParams.entrySet()) {
            Assert.assertEquals(param.getValue(), uri.getQueryParameter(param.getKey()));
        }
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    @Restriction(DeviceFormFactor.PHONE) // see crbug.com/581268
    public void testLoadUrlService() {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return mTemplateUrlService.isLoaded();
                            }
                        }));

        // Add another load listener and ensure that is notified without needing to call load()
        // again.
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTemplateUrlService.registerLoadListener(
                            new LoadListener() {
                                @Override
                                public void onTemplateUrlServiceLoaded() {
                                    observerNotified.set(true);
                                }
                            });
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return observerNotified.get();
                },
                "Observer wasn't notified of TemplateUrlService load.");
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetAndGetSearchEngine() {
        waitForTemplateUrlServiceToLoad();

        List<TemplateUrl> searchEngines = getSearchEngines(mTemplateUrlService);
        // Ensure known state of default search index before running test.
        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                searchEngines, defaultSearchEngine, /* isEeaChoiceCountry= */ false);

        // Outside of the EEA, where prepopulated engines are always sorted by ID, Google has the
        // lowest ID and will be at the index 0 in the sorted list.
        Assert.assertEquals(defaultSearchEngine.getPrepopulatedId(), /* Google's ID: */ 1);
        Assert.assertEquals(searchEngines.get(0), defaultSearchEngine);

        // Set search engine index and verify it stuck.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "There must be more than one search engine to change searchEngines",
                            searchEngines.size() > 1);
                    mTemplateUrlService.setSearchEngine(searchEngines.get(1).getKeyword());
                });

        defaultSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        Assert.assertEquals(searchEngines.get(1), defaultSearchEngine);
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_CreateNew_AllProvided() {
        waitForTemplateUrlServiceToLoad();

        // Adding Play API search engine should succeed.
        Assert.assertTrue(
                setPlayAPISearchEngine(
                        mTemplateUrlService,
                        "SearchEngine1",
                        "keyword1",
                        PLAY_API_SEARCH_URL,
                        PLAY_API_SUGGEST_URL,
                        PLAY_API_FAVICON_URL,
                        PLAY_API_NEW_TAB_URL,
                        PLAY_API_IMAGE_URL,
                        PLAY_API_IMAGE_POST_PARAM,
                        PLAY_API_IMAGE_TRANSLATE_URL,
                        PLAY_API_IMAGE_TRANSLATE_SOURCE_KEY,
                        PLAY_API_IMAGE_TRANSLATE_DEST_KEY));

        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        Assert.assertEquals("keyword1", defaultSearchEngine.getKeyword());
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        Assert.assertEquals(PLAY_API_SEARCH_URL, defaultSearchEngine.getURL());
        Assert.assertEquals(PLAY_API_NEW_TAB_URL, defaultSearchEngine.getNewTabURL());
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_UpdatePrepopulated() {
        // TODO(b/360885010) Do not run the test on chrome-branded builds because these don't
        // use fieldtrial testing.
        if (BuildConfig.IS_CHROME_BRANDED) return;

        waitForTemplateUrlServiceToLoad();

        TemplateUrl originalSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        String originalKeyword = originalSearchEngine.getKeyword();
        Assert.assertTrue(originalSearchEngine.getIsPrepopulated());
        int searchEnginesCountBefore = getSearchEngineCount(mTemplateUrlService);

        // Adding Play API search engine with the same keyword should succeed.
        Assert.assertTrue(
                setPlayAPISearchEngine(
                        mTemplateUrlService,
                        originalSearchEngine.getShortName(),
                        // Note: matching keyword should trigger reconciliation.
                        originalKeyword,
                        PLAY_API_SEARCH_URL,
                        PLAY_API_SUGGEST_URL,
                        PLAY_API_FAVICON_URL,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null));

        TemplateUrl updatedSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        Assert.assertEquals(originalKeyword, updatedSearchEngine.getKeyword());
        // Chrome should promote built-in definitions.
        Assert.assertEquals(originalSearchEngine.getURL(), updatedSearchEngine.getURL());
        // Still appears as prepopulated.
        Assert.assertTrue(updatedSearchEngine.getIsPrepopulated());

        // We need to ensure that from perspective of Java, the number of search engines is the same
        // even though the update didn't happen in place.
        int searchEnginesCountAfter = getSearchEngineCount(mTemplateUrlService);
        Assert.assertEquals(searchEnginesCountBefore, searchEnginesCountAfter);
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_UpdateExisting() {
        waitForTemplateUrlServiceToLoad();

        // Add regular search engine. It will be used to test conflict with Play API search engine.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTemplateUrlService.addSearchEngineForTesting("keyword1", 0);
                });

        // Adding Play API search engine with the same keyword should succeed.
        Assert.assertTrue(
                setPlayAPISearchEngine(
                        mTemplateUrlService,
                        "SearchEngine1",
                        "keyword1",
                        PLAY_API_SEARCH_URL,
                        PLAY_API_SUGGEST_URL,
                        PLAY_API_FAVICON_URL,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null));

        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        Assert.assertEquals("keyword1", defaultSearchEngine.getKeyword());
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        Assert.assertEquals(PLAY_API_SEARCH_URL, defaultSearchEngine.getURL());

        // Adding Play API search engine again should replace the previous one.
        String otherSearchUrl = "https://other.play.search.engine?q={searchTerms}";
        Assert.assertTrue(
                setPlayAPISearchEngine(
                        mTemplateUrlService,
                        "SearchEngine2",
                        "keyword2",
                        otherSearchUrl,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null,
                        null));

        defaultSearchEngine = getDefaultSearchEngine(mTemplateUrlService);
        Assert.assertEquals("keyword2", defaultSearchEngine.getKeyword());
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        Assert.assertEquals(otherSearchUrl, defaultSearchEngine.getURL());
        Assert.assertThat(
                getSearchEngines(mTemplateUrlService).stream()
                        .map(TemplateUrl::getKeyword)
                        .collect(Collectors.toList()),
                not(hasItem("keyword1")));
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testGetUrlForSearchQuery() throws ExecutionException {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return mTemplateUrlService.isLoaded();
                            }
                        }));

        validateSearchQuery("cat", null, null);
        Map<String, String> params = new HashMap();
        params.put("xyz", "a");
        validateSearchQuery("cat", new ArrayList<String>(Arrays.asList("xyz=a")), params);
        params.put("abc", "b");
        validateSearchQuery("cat", new ArrayList<String>(Arrays.asList("xyz=a", "abc=b")), params);
    }

    private boolean setPlayAPISearchEngine(
            TemplateUrlService templateUrlService,
            String name,
            String keyword,
            String searchUrl,
            String suggestUrl,
            String faviconUrl,
            String newTabUrl,
            String imageUrl,
            String imageUrlPostParams,
            String imageTranslateUrl,
            String imageTranslateSourceLanguageParamKey,
            String imageTranslateTargetLanguageParamKey) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return templateUrlService.setPlayAPISearchEngine(
                            name,
                            keyword,
                            searchUrl,
                            suggestUrl,
                            faviconUrl,
                            newTabUrl,
                            imageUrl,
                            imageUrlPostParams,
                            imageTranslateUrl,
                            imageTranslateSourceLanguageParamKey,
                            imageTranslateTargetLanguageParamKey);
                });
    }

    private TemplateUrl getDefaultSearchEngine(TemplateUrlService templateUrlService) {
        return ThreadUtils.runOnUiThreadBlocking(
                templateUrlService::getDefaultSearchEngineTemplateUrl);
    }

    private List<TemplateUrl> getSearchEngines(TemplateUrlService templateUrlService) {
        return ThreadUtils.runOnUiThreadBlocking(templateUrlService::getTemplateUrls);
    }

    private int getSearchEngineCount(TemplateUrlService templateUrlService) {
        return getSearchEngines(templateUrlService).size();
    }

    private void waitForTemplateUrlServiceToLoad() {
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        final LoadListener listener =
                new LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        observerNotified.set(true);
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTemplateUrlService.registerLoadListener(listener);
                    mTemplateUrlService.load();
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return observerNotified.get();
                },
                "Observer wasn't notified of TemplateUrlService load.");
    }
}
