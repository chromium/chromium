// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineAdapter;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for Chrome on Android's usage of the TemplateUrlService API.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TemplateUrlServiceTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

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

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testUrlForContextualSearchQueryValid() throws ExecutionException {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return TemplateUrlServiceFactory.get().isLoaded();
            }
        }));

        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_SINGLE_REQUEST_PROTOCOL);
    }

    private void validateQuery(final String query, final String alternative, final boolean prefetch,
            final String protocolVersion)
            throws ExecutionException {
        GURL result = TestThreadUtils.runOnUiThreadBlocking(new Callable<GURL>() {
            @Override
            public GURL call() {
                return TemplateUrlServiceFactory.get().getUrlForContextualSearchQuery(
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

    private void validateSearchQuery(final String query, final List<String> searchParams,
            final Map<String, String> expectedParams) throws ExecutionException {
        String result = TestThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return TemplateUrlServiceFactory.get().getUrlForSearchQuery(query, searchParams);
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
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // see crbug.com/581268
    public void testLoadUrlService() {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return TemplateUrlServiceFactory.get().isLoaded();
            }
        }));

        // Add another load listener and ensure that is notified without needing to call load()
        // again.
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlService service = TemplateUrlServiceFactory.get();
            service.registerLoadListener(new LoadListener() {
                @Override
                public void onTemplateUrlServiceLoaded() {
                    observerNotified.set(true);
                }
            });
        });
        CriteriaHelper.pollInstrumentationThread(() -> {
            return observerNotified.get();
        }, "Observer wasn't notified of TemplateUrlService load.");
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetAndGetSearchEngine() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        List<TemplateUrl> searchEngines = getSearchEngines(templateUrlService);
        // Ensure known state of default search index before running test.
        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(searchEngines, defaultSearchEngine);
        Assert.assertEquals(searchEngines.get(0), defaultSearchEngine);

        // Set search engine index and verified it stuck.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("There must be more than one search engine to change searchEngines",
                    searchEngines.size() > 1);
            templateUrlService.setSearchEngine(searchEngines.get(1).getKeyword());
        });

        defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        Assert.assertEquals(searchEngines.get(1), defaultSearchEngine);
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_CreateNew() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        // Adding Play API search engine should succeed.
        Assert.assertTrue(setPlayAPISearchEngine(templateUrlService, "SearchEngine1", "keyword1",
                PLAY_API_SEARCH_URL, PLAY_API_SUGGEST_URL, PLAY_API_FAVICON_URL, true));

        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        Assert.assertEquals("keyword1", defaultSearchEngine.getKeyword());
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        Assert.assertEquals(PLAY_API_SEARCH_URL, defaultSearchEngine.getURL());
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_UpdatePrepopulated() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        String originalKeyword = defaultSearchEngine.getKeyword();
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        int searchEnginesCountBefore = getSearchEngineCount(templateUrlService);

        // Adding Play API search engine with the same keyword should succeed.
        Assert.assertTrue(setPlayAPISearchEngine(templateUrlService,
                defaultSearchEngine.getShortName(), originalKeyword, PLAY_API_SEARCH_URL,
                PLAY_API_SUGGEST_URL, PLAY_API_FAVICON_URL, true));

        defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        Assert.assertEquals(originalKeyword, defaultSearchEngine.getKeyword());
        Assert.assertEquals(PLAY_API_SEARCH_URL, defaultSearchEngine.getURL());
        // Still appears as prepopulated.
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());

        // We need to ensure that from perspective of Java, the number of search engines is the same
        // even though the update didn't happen in place.
        int searchEnginesCountAfter = getSearchEngineCount(templateUrlService);
        Assert.assertEquals(searchEnginesCountBefore, searchEnginesCountAfter);
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetPlayAPISearchEngine_UpdateExisting() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        // Add regular search engine. It will be used to test conflict with Play API search engine.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { templateUrlService.addSearchEngineForTesting("keyword1", 0); });

        // Adding Play API search engine with the same keyword should succeed.
        Assert.assertTrue(setPlayAPISearchEngine(templateUrlService, "SearchEngine1", "keyword1",
                PLAY_API_SEARCH_URL, PLAY_API_SUGGEST_URL, PLAY_API_FAVICON_URL, true));

        TemplateUrl defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        Assert.assertEquals("keyword1", defaultSearchEngine.getKeyword());
        Assert.assertTrue(defaultSearchEngine.getIsPrepopulated());
        Assert.assertEquals(PLAY_API_SEARCH_URL, defaultSearchEngine.getURL());

        // Adding Play API search engine again should fail.
        Assert.assertFalse(setPlayAPISearchEngine(templateUrlService, "SearchEngine2", "keyword2",
                PLAY_API_SEARCH_URL, PLAY_API_SUGGEST_URL, PLAY_API_FAVICON_URL, true));

        defaultSearchEngine = getDefaultSearchEngine(templateUrlService);
        Assert.assertEquals("keyword1", defaultSearchEngine.getKeyword());
    }

    @Test
    @SmallTest
    @Feature({"SearchEngines"})
    public void testGetUrlForSearchQuery() throws ExecutionException {
        waitForTemplateUrlServiceToLoad();

        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return TemplateUrlServiceFactory.get().isLoaded();
            }
        }));

        validateSearchQuery("cat", null, null);
        Map<String, String> params = new HashMap();
        params.put("xyz", "a");
        validateSearchQuery("cat", new ArrayList<String>(Arrays.asList("xyz=a")), params);
        params.put("abc", "b");
        validateSearchQuery("cat", new ArrayList<String>(Arrays.asList("xyz=a", "abc=b")), params);
    }

    private boolean setPlayAPISearchEngine(TemplateUrlService templateUrlService, String name,
            String keyword, String searchUrl, String suggestUrl, String faviconUrl,
            boolean setAsDefault) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return templateUrlService.setPlayAPISearchEngine(
                    name, keyword, searchUrl, suggestUrl, faviconUrl, setAsDefault);
        });
    }

    private TemplateUrl getDefaultSearchEngine(TemplateUrlService templateUrlService) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                templateUrlService::getDefaultSearchEngineTemplateUrl);
    }

    private List<TemplateUrl> getSearchEngines(TemplateUrlService templateUrlService) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                templateUrlService::getTemplateUrls);
    }

    private int getSearchEngineCount(TemplateUrlService templateUrlService) {
        return getSearchEngines(templateUrlService).size();
    }

    private TemplateUrlService waitForTemplateUrlServiceToLoad() {
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        final LoadListener listener = new LoadListener() {
            @Override
            public void onTemplateUrlServiceLoaded() {
                observerNotified.set(true);
            }
        };
        final TemplateUrlService templateUrlService =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        new Callable<TemplateUrlService>() {
                            @Override
                            public TemplateUrlService call() {
                                TemplateUrlService service = TemplateUrlServiceFactory.get();
                                service.registerLoadListener(listener);
                                service.load();
                                return service;
                            }
                        });

        CriteriaHelper.pollInstrumentationThread(() -> {
            return observerNotified.get();
        }, "Observer wasn't notified of TemplateUrlService load.");
        return templateUrlService;
    }
}
