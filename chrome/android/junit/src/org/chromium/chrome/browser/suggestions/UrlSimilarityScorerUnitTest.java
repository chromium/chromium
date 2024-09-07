// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.suggestions.UrlSimilarityScorer.EXACT;
import static org.chromium.chrome.browser.suggestions.UrlSimilarityScorer.MISMATCHED;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.suggestions.UrlSimilarityScorer.MatchResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link UrlSimilarityScorerUnitTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlSimilarityScorerUnitTest {

    /** Interface to reduce test boilerplate. */
    interface StringToInt {
        public abstract int run(String s);
    }

    /** Interface to reduce test boilerplate. */
    interface StringStringToInteger {
        public abstract Integer run(String s1, String s2);
    }

    /** Interface to reduce test boilerplate. */
    interface Boolean4ToString {
        public abstract @Nullable String run(boolean b1, boolean b2, boolean b3, boolean b4);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @SmallTest
    public void testCanonicalizeHost() {
        assertEquals("example.com", UrlSimilarityScorer.canonicalizeHost("example.com"));
        assertEquals("example.com", UrlSimilarityScorer.canonicalizeHost("www.example.com"));
        assertEquals("example.com", UrlSimilarityScorer.canonicalizeHost("m.www.example.com"));
        assertEquals(
                "example.com",
                UrlSimilarityScorer.canonicalizeHost("www.mobile.touch.example.com"));
        assertEquals("example.www.com", UrlSimilarityScorer.canonicalizeHost("example.www.com"));
        assertEquals("", UrlSimilarityScorer.canonicalizeHost(""));
        assertEquals(".....", UrlSimilarityScorer.canonicalizeHost("....."));
        assertEquals("x.m.com", UrlSimilarityScorer.canonicalizeHost("m.m.m.m.x.m.com"));
        assertEquals("", UrlSimilarityScorer.canonicalizeHost("mobile."));
        assertEquals("mobile", UrlSimilarityScorer.canonicalizeHost("mobile"));
    }

    @Test
    @SmallTest
    public void testEnsureSlashSentinel() {
        assertEquals("/", UrlSimilarityScorer.ensureSlashSentinel(""));
        assertEquals("/foo/", UrlSimilarityScorer.ensureSlashSentinel("foo"));
        assertEquals("/foo/", UrlSimilarityScorer.ensureSlashSentinel("/foo"));
        assertEquals("/foo/", UrlSimilarityScorer.ensureSlashSentinel("foo/"));
        assertEquals("/foo/", UrlSimilarityScorer.ensureSlashSentinel("/foo/"));
        assertEquals("/", UrlSimilarityScorer.ensureSlashSentinel("/"));
        assertEquals("//", UrlSimilarityScorer.ensureSlashSentinel("//"));
        assertEquals("/////////", UrlSimilarityScorer.ensureSlashSentinel("/////////"));
        assertEquals("/foo/bar/", UrlSimilarityScorer.ensureSlashSentinel("foo/bar"));
        assertEquals("/foo/bar/", UrlSimilarityScorer.ensureSlashSentinel("/foo/bar"));
        assertEquals("/foo/bar/", UrlSimilarityScorer.ensureSlashSentinel("foo/bar/"));
        assertEquals("/foo/bar/", UrlSimilarityScorer.ensureSlashSentinel("/foo/bar/"));
        assertEquals("/a/B/cc/d1/2e/", UrlSimilarityScorer.ensureSlashSentinel("a/B/cc/d1/2e"));
        assertEquals("/aBccd12e/", UrlSimilarityScorer.ensureSlashSentinel("aBccd12e"));
    }

    @Test
    @SmallTest
    public void testGetPathAncestorDepth() {
        StringStringToInteger f =
                (String ancestorPath, String path) ->
                        UrlSimilarityScorer.getPathAncestralDepth(ancestorPath, path);

        // Identical paths.
        assertEquals(Integer.valueOf(0), f.run("/", "/"));
        assertEquals(Integer.valueOf(0), f.run("/foo/", "/foo/"));
        assertEquals(Integer.valueOf(0), f.run("/a/b/", "/a/b/"));
        assertEquals(Integer.valueOf(0), f.run("/a/bb/ccc/", "/a/bb/ccc/"));
        assertEquals(Integer.valueOf(0), f.run("/a/bb/c/d/e/f/g/h/", "/a/bb/c/d/e/f/g/h/"));
        assertEquals(Integer.valueOf(0), f.run("/aaaAAaaaaaaazz/", "/aaaAAaaaaaaazz/"));
        assertEquals(
                Integer.valueOf(0),
                f.run(
                        "/a/a/a/a/A/a/a/a/a/x/a/a/1/a/a/bB/a/a/a/",
                        "/a/a/a/a/A/a/a/a/a/x/a/a/1/a/a/bB/a/a/a/"));

        // Reject directory / file prefix.
        assertNull(f.run("/a/", "/alpha/"));
        assertNull(f.run("/alpha/", "/a/"));
        assertNull(f.run("/a/b/", "/a/bbb/"));

        // Root as ancestor.
        assertEquals(Integer.valueOf(1), f.run("/", "/a/"));
        assertEquals(Integer.valueOf(2), f.run("/", "/a/b/"));
        assertEquals(Integer.valueOf(3), f.run("/", "/a/bb/c/"));
        assertEquals(Integer.valueOf(4), f.run("/", "/a/bbb/cc/d/"));
        assertEquals(Integer.valueOf(5), f.run("/", "/a/b/c/d/e/"));

        // Reversed.
        assertNull(f.run("/a/", "/"));
        assertNull(f.run("/a/b/", "/"));
        assertNull(f.run("/a/bb/c/", "/"));
        assertNull(f.run("/a/bbb/cc/d/", "/"));
        assertNull(f.run("/a/b/c/d/e/", "/"));

        // Ancestor 1.
        assertEquals(Integer.valueOf(1), f.run("/a/", "/a/b/"));
        assertEquals(Integer.valueOf(1), f.run("/a/bb/", "/a/bb/c/"));
        assertEquals(Integer.valueOf(1), f.run("/a/bbb/cc/", "/a/bbb/cc/d/"));
        assertEquals(Integer.valueOf(1), f.run("/a/b/c/d/", "/a/b/c/d/e/"));

        // Ancestor 2.
        assertEquals(Integer.valueOf(2), f.run("/a/", "/a/bb/c/"));
        assertEquals(Integer.valueOf(2), f.run("/a/bbb/", "/a/bbb/cc/d/"));
        assertEquals(Integer.valueOf(2), f.run("/a/b/c/", "/a/b/c/d/e/"));

        // Deep directories.
        assertEquals(Integer.valueOf(20), f.run("/", "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));
        assertEquals(Integer.valueOf(19), f.run("/", "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));
        assertEquals(Integer.valueOf(18), f.run("/a/", "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));
        assertEquals(
                Integer.valueOf(1),
                f.run(
                        "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/",
                        "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));

        // Non-ancestor.
        assertNull(f.run("/a/", "/A/"));
        assertNull(f.run("/a/b/", "/a/"));
        assertNull(f.run("/a/b/", "/b/"));
        assertNull(f.run("/a/b/", "/a/c/"));
        assertNull(f.run("/a/b/", "/a/b1/"));
        assertNull(f.run("/a/b/", "/b/a/"));
        assertNull(f.run("/a/b/", "/aa/b/"));
        assertNull(f.run("/aa/b/", "/a/b/"));
        assertNull(f.run("/aa/b/", "/a/b/c/"));
        assertNull(f.run("/a/b/", "/b/a/c/"));
        assertNull(f.run("/a/b/", "/c/a/b/"));
        assertNull(f.run("/a/b/", "/a/c/b/"));
        assertNull(f.run("/x/", "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));
        assertNull(
                f.run(
                        "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/",
                        "/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/"));

        // Empty directory names work, even though they're unrealistic.
        assertEquals(Integer.valueOf(1), f.run("/", "//"));
        assertEquals(Integer.valueOf(8), f.run("/", "/////////"));
        assertNull(f.run("//", "/"));
        assertEquals(Integer.valueOf(5), f.run("////", "/////////"));
    }

    @Test
    @SmallTest
    public void testGetSimilarityStrict() {
        GURL testUrl = new GURL("https://example.com");
        UrlSimilarityScorer scorer = makeExactMatchScorer(testUrl);
        StringToInt f = (String urlStr) -> scorer.scoreSimilarity(new GURL(urlStr));
        assertEquals(EXACT, f.run("https://example.com"));
        assertEquals(MISMATCHED, f.run("http://example.com"));
        assertEquals(MISMATCHED, f.run("https://example.com:8000"));
        assertEquals(MISMATCHED, f.run("https://www.example.com"));
        assertEquals(MISMATCHED, f.run("https://example.com/path"));
        assertEquals(MISMATCHED, f.run("https://example.com/path/"));
        assertEquals(MISMATCHED, f.run("https://example.com/?query=1"));
        assertEquals(MISMATCHED, f.run("https://example.com/#ref"));
        assertEquals(MISMATCHED, f.run("https://www.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.example.com/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path/?query=1#ref"));
    }

    @Test
    @SmallTest
    public void testGetSimilarityIntermediate() {
        GURL testUrl = new GURL("https://example.com");
        UrlSimilarityScorer scorer = makeLaxSchemeHostRefScorer(testUrl);
        StringToInt f = (String urlStr) -> scorer.scoreSimilarity(new GURL(urlStr));
        assertEquals(EXACT, f.run("https://example.com"));
        assertEquals(MISMATCHED, f.run("http://example.com"));
        assertEquals(MISMATCHED, f.run("https://example.com:8000"));
        assertEquals(993, f.run("https://www.example.com"));
        assertEquals(MISMATCHED, f.run("https://example.com/path"));
        assertEquals(MISMATCHED, f.run("https://example.com/path/"));
        assertEquals(MISMATCHED, f.run("https://example.com/?query=1"));
        assertEquals(992, f.run("https://example.com/#hash"));
        assertEquals(MISMATCHED, f.run("https://www.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.example.com/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path/?query=1#ref"));
    }

    @Test
    @SmallTest
    public void testGetSimilarityLax_KeyUrlRoot() {
        GURL testUrl = new GURL("https://example.com");
        UrlSimilarityScorer scorer = makeLaxSchemeHostRefQueryPathScorer(testUrl);
        StringToInt f = (String urlStr) -> scorer.scoreSimilarity(new GURL(urlStr));
        assertEquals(EXACT, f.run("https://example.com"));
        assertEquals(MISMATCHED, f.run("http://example.com"));
        assertEquals(MISMATCHED, f.run("https://example.com:8000"));
        assertEquals(993, f.run("https://www.example.com"));
        assertEquals(983, f.run("https://example.com/path")); // File.
        assertEquals(983, f.run("https://example.com/path/")); // Directory.
        assertEquals(903, f.run("https://example.com/path/a/b/c/d/e/f/g/h"));
        assertEquals(991, f.run("https://example.com/?query=1"));
        assertEquals(992, f.run("https://example.com/#ref"));
        assertEquals(980, f.run("https://touch.example.com/path?query=1#ref"));
        assertEquals(980, f.run("https://www.example.com/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path/?query=1#ref"));

        String deepPath = "a/".repeat(200);
        assertEquals(13, f.run("https://www.example.com/" + deepPath));
        assertEquals(12, f.run("https://www.example.com/" + deepPath + "?#ref"));
        assertEquals(11, f.run("https://www.example.com/" + deepPath + "?query=1"));
        assertEquals(10, f.run("https://www.example.com/" + deepPath + "?query=1#ref"));
    }

    @Test
    @SmallTest
    public void testGetSimilarityLax_KeyUrlDirectory() {
        GURL testUrl = new GURL("https://m.example.com/path/?query=1#ref");
        UrlSimilarityScorer scorer = makeLaxSchemeHostRefQueryPathScorer(testUrl);
        StringToInt f = (String urlStr) -> scorer.scoreSimilarity(new GURL(urlStr));
        assertEquals(MISMATCHED, f.run("https://example.com"));
        assertEquals(MISMATCHED, f.run("http://m.example.com/path/?query=1#ref"));

        assertEquals(MISMATCHED, f.run("https://m.example.com:8000/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.example.com"));
        assertEquals(990, f.run("https://example.com/path")); // File.
        assertEquals(990, f.run("https://example.com/path/")); // Directory.
        assertEquals(910, f.run("https://example.com/path/a/b/c/d/e/f/g/h"));
        assertEquals(MISMATCHED, f.run("https://example.com/?query=1"));
        assertEquals(MISMATCHED, f.run("https://example.com/#ref"));
        assertEquals(992, f.run("https://example.com/path?query=1")); // File.
        assertEquals(991, f.run("https://example.com/path#ref")); // File.
        assertEquals(992, f.run("https://example.com/path/?query=1")); // Directory.
        assertEquals(991, f.run("https://example.com/path/#ref")); // Directory.
        assertEquals(993, f.run("https://www.example.com/path?query=1#ref")); // File.
        assertEquals(EXACT, f.run("https://m.example.com/path/?query=1#ref"));
        assertEquals(993, f.run("https://touch.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com/path/?query=1#ref"));

        // Repeat for https -> http.
        assertEquals(MISMATCHED, f.run("http://m.example.com:8000/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.example.com"));
        assertEquals(MISMATCHED, f.run("http://example.com/path")); // File.
        assertEquals(MISMATCHED, f.run("http://example.com/path/")); // Directory.
        assertEquals(MISMATCHED, f.run("http://example.com/path/a/b/c/d/e/f/g/h"));
        assertEquals(MISMATCHED, f.run("http://example.com/?query=1"));
        assertEquals(MISMATCHED, f.run("http://example.com/#ref"));
        assertEquals(MISMATCHED, f.run("http://example.com/path?query=1")); // File.
        assertEquals(MISMATCHED, f.run("http://example.com/path#ref")); // File.
        assertEquals(MISMATCHED, f.run("http://example.com/path/?query=1")); // Directory.
        assertEquals(MISMATCHED, f.run("http://example.com/path/#ref")); // Directory.
        assertEquals(MISMATCHED, f.run("http://www.example.com/path?query=1#ref")); // File.
        assertEquals(MISMATCHED, f.run("http://m.example.com/path/?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://touch.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com/path/?query=1#ref"));
    }

    @Test
    @SmallTest
    public void testGetSimilarityLax_KeyUrlFile() {
        GURL testUrl = new GURL("http://touch.example.com:1234/path?query=1#ref");
        UrlSimilarityScorer scorer = makeLaxSchemeHostRefQueryPathScorer(testUrl);
        StringToInt f = (String urlStr) -> scorer.scoreSimilarity(new GURL(urlStr));
        assertEquals(MISMATCHED, f.run("http://example.com:1234"));
        assertEquals(993, f.run("https://touch.example.com:1234/path?query=1#ref"));

        assertEquals(MISMATCHED, f.run("http://touch.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.example.com:1234"));
        assertEquals(990, f.run("http://example.com:1234/path")); // File.
        assertEquals(990, f.run("http://example.com:1234/path/")); // Directory.
        assertEquals(910, f.run("http://example.com:1234/path/a/b/c/d/e/f/g/h"));
        assertEquals(MISMATCHED, f.run("http://example.com:1234/?query=1"));
        assertEquals(MISMATCHED, f.run("http://example.com:1234/#ref"));
        assertEquals(992, f.run("http://example.com:1234/path?query=1")); // File.
        assertEquals(991, f.run("http://example.com:1234/path#ref")); // File.
        assertEquals(992, f.run("http://example.com:1234/path/?query=1")); // Directory.
        assertEquals(991, f.run("http://example.com:1234/path/#ref")); // Directory.
        assertEquals(993, f.run("http://www.example.com:1234/path?query=1#ref")); // File.
        assertEquals(993, f.run("http://m.example.com:1234/path/?query=1#ref"));
        assertEquals(EXACT, f.run("http://touch.example.com:1234/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com:1234"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com:1234/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("http://www.not-example.com:1234/path/?query=1#ref"));

        // Repeat for http -> https.
        assertEquals(MISMATCHED, f.run("https://touch.example.com/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.example.com:1234"));
        assertEquals(990, f.run("https://example.com:1234/path")); // File.
        assertEquals(990, f.run("https://example.com:1234/path/")); // Directory.
        assertEquals(910, f.run("https://example.com:1234/path/a/b/c/d/e/f/g/h"));
        assertEquals(MISMATCHED, f.run("https://example.com:1234/?query=1"));
        assertEquals(MISMATCHED, f.run("https://example.com:1234/#ref"));
        assertEquals(992, f.run("https://example.com:1234/path?query=1")); // File.
        assertEquals(991, f.run("https://example.com:1234/path#ref")); // File.
        assertEquals(992, f.run("https://example.com:1234/path/?query=1")); // Directory.
        assertEquals(991, f.run("https://example.com:1234/path/#ref")); // Directory.
        assertEquals(993, f.run("https://www.example.com:1234/path?query=1#ref")); // File.
        assertEquals(993, f.run("https://m.example.com:1234/path/?query=1#ref"));
        assertEquals(993, f.run("https://touch.example.com:1234/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com:1234"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com:1234/path?query=1#ref"));
        assertEquals(MISMATCHED, f.run("https://www.not-example.com:1234/path/?query=1#ref"));
    }

    @Test
    @SmallTest
    public void testFindTabWithMostSimilarUrl() {
        int bad = TabList.INVALID_TAB_INDEX;

        ArrayList<GURL> candidateUrls = new ArrayList<GURL>();
        candidateUrls.add(new GURL("https://not-example.com"));
        candidateUrls.add(new GURL("https://www.example.com/path?query#ref"));
        candidateUrls.add(new GURL("https://www.example.com/?query#ref"));
        candidateUrls.add(new GURL("https://www.example.com/#ref"));
        candidateUrls.add(new GURL("https://www.example.com/"));
        candidateUrls.add(new GURL("https://example.com/"));

        GURL keyUrl1 = new GURL("https://example.com");
        assertArrayEquals(
                new int[] {bad, bad, bad, bad, bad, 5},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeExactMatchScorer(keyUrl1)));
        assertArrayEquals(
                new int[] {bad, bad, bad, 3, 4, 5},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeLaxSchemeHostRefScorer(keyUrl1)));
        assertArrayEquals(
                new int[] {bad, bad, 2, 3, 4, 5},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryScorer(keyUrl1)));
        assertArrayEquals(
                new int[] {bad, 1, 2, 3, 4, 5},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryPathScorer(keyUrl1)));

        GURL keyUrl2 = new GURL("https://www.example.com/#ref");
        assertArrayEquals(
                new int[] {bad, bad, bad, 3, 3, 3},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeExactMatchScorer(keyUrl2)));
        assertArrayEquals(
                new int[] {bad, bad, bad, 3, 3, 3},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeLaxSchemeHostRefScorer(keyUrl2)));
        assertArrayEquals(
                new int[] {bad, bad, 2, 3, 3, 3},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryScorer(keyUrl2)));
        assertArrayEquals(
                new int[] {bad, 1, 2, 3, 3, 3},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryPathScorer(keyUrl2)));

        GURL keyUrl3 = new GURL("https://m.example.com/?query");
        assertArrayEquals(
                new int[] {bad, bad, bad, bad, bad, bad},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeExactMatchScorer(keyUrl3)));
        assertArrayEquals(
                new int[] {bad, bad, 2, 2, 2, 2},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeLaxSchemeHostRefScorer(keyUrl3)));
        assertArrayEquals(
                new int[] {bad, bad, 2, 2, 2, 2},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryScorer(keyUrl3)));
        assertArrayEquals(
                new int[] {bad, 1, 2, 2, 2, 2},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryPathScorer(keyUrl3)));

        GURL keyUrl4 = new GURL("https://mobile.example.com/path");
        assertArrayEquals(
                new int[] {bad, bad, bad, bad, bad, bad},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeExactMatchScorer(keyUrl4)));
        assertArrayEquals(
                new int[] {bad, bad, bad, bad, bad, bad},
                batchFindTabWithMostSimilarUrl(candidateUrls, makeLaxSchemeHostRefScorer(keyUrl4)));
        assertArrayEquals(
                new int[] {bad, 1, 1, 1, 1, 1},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryScorer(keyUrl4)));
        assertArrayEquals(
                new int[] {bad, 1, 1, 1, 1, 1},
                batchFindTabWithMostSimilarUrl(
                        candidateUrls, makeLaxSchemeHostRefQueryPathScorer(keyUrl4)));
    }

    @Test
    @SmallTest
    public void testGetHistogramStrictnessSuffix() {
        GURL url = new GURL("https://www.example.com");
        Boolean4ToString f =
                (boolean laxSchemeHost, boolean laxRef, boolean laxQuery, boolean laxPath) -> {
                    return new UrlSimilarityScorer(url, laxSchemeHost, laxRef, laxQuery, laxPath)
                            .getHistogramStrictnessSuffix();
                };
        assertEquals("Strict", f.run(false, false, false, false));
        assertNull(f.run(false, false, false, true));
        assertNull(f.run(false, false, true, false));
        assertNull(f.run(false, false, true, true));
        assertNull(f.run(false, true, false, false));
        assertNull(f.run(false, true, false, true));
        assertNull(f.run(false, true, true, false));
        assertNull(f.run(false, true, true, true));
        assertNull(f.run(true, false, false, false));
        assertNull(f.run(true, false, false, true));
        assertNull(f.run(true, false, true, false));
        assertNull(f.run(true, false, true, true));
        assertEquals("LaxUpToRef", f.run(true, true, false, false));
        assertNull(f.run(true, true, false, true));
        assertEquals("LaxUpToQuery", f.run(true, true, true, false));
        assertEquals("LaxUpToPath", f.run(true, true, true, true));
    }

    private TabList createTabList(List<GURL> urlList) {
        TabList tabList = mock(TabList.class);
        for (int i = 0; i < urlList.size(); ++i) {
            Tab tab = mock(Tab.class);
            // Use lenient() since some of these might not get called, findTabWithMostSimilarUrl()'s
            // early-exit path executes.
            lenient().doReturn(urlList.get(i)).when(tab).getUrl();
            lenient().doReturn(tab).when(tabList).getTabAt(i);
        }
        doReturn(urlList.size()).when(tabList).getCount();
        return tabList;
    }

    private UrlSimilarityScorer makeExactMatchScorer(GURL keyUrl) {
        return new UrlSimilarityScorer(
                keyUrl,
                /* laxSchemeHost= */ false,
                /* laxRef= */ false,
                /* laxQuery= */ false,
                /* laxPath= */ false);
    }

    private UrlSimilarityScorer makeLaxSchemeHostRefScorer(GURL keyUrl) {
        return new UrlSimilarityScorer(
                keyUrl,
                /* laxSchemeHost= */ true,
                /* laxRef= */ true,
                /* laxQuery= */ false,
                /* laxPath= */ false);
    }

    private UrlSimilarityScorer makeLaxSchemeHostRefQueryScorer(GURL keyUrl) {
        return new UrlSimilarityScorer(
                keyUrl,
                /* laxSchemeHost= */ true,
                /* laxRef= */ true,
                /* laxQuery= */ true,
                /* laxPath= */ false);
    }

    private UrlSimilarityScorer makeLaxSchemeHostRefQueryPathScorer(GURL keyUrl) {
        return new UrlSimilarityScorer(
                keyUrl,
                /* laxSchemeHost= */ true,
                /* laxRef= */ true,
                /* laxQuery= */ true,
                /* laxPath= */ true);
    }

    /**
     * Runs `scorer.findTabWithMostSimilarUrl()` against TabList formed from successive non-empty
     * prefixes of `candidateUrls`.
     */
    private int[] batchFindTabWithMostSimilarUrl(
            List<GURL> candidateUrls, UrlSimilarityScorer scorer) {
        int n = candidateUrls.size();
        int[] bestIndices = new int[n];
        for (int i = 0; i < n; ++i) {
            TabList tabList = createTabList(candidateUrls.subList(0, i + 1));
            MatchResult result = scorer.findTabWithMostSimilarUrl(tabList);
            bestIndices[i] = result.index;
        }
        return bestIndices;
    }
}
