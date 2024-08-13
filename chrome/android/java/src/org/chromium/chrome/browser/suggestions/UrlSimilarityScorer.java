// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;

import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Heuristically scores the similarity between a "key" URL with one or more "candidate" URLs. The
 * main use is to find the best match among "candidate" URLs from a TabList.
 */
public class UrlSimilarityScorer {
    // Value for candidate URL rejection, set to negative since match scores are non-negative.
    public static final int MISMATCHED = -1;
    // Identical matches beat all, and so is assigned maximal value.
    public static final int IDENTICAL = Integer.MAX_VALUE;

    public static final int PATH_MATCH_MAX_SCORE = 99;
    public static final int SCORE_PATH_MATCH_MULTIPLIER = 10;
    public static final int SCORE_QUERY_MATCH = 2;
    public static final int SCORE_REF_MATCH = 1;

    // These are the same ones used in VisitSegmentDatabase::ComputeSegmentName().
    private static Set<String> sDiscardableHostPrefixes =
            new HashSet<>(Arrays.asList("www", "m", "mobile", "touch"));

    private final GURL mKeyUrl;
    // Match parameters, ordered so intended usage would specify "true" before "false".
    private final boolean mLaxHost;
    private final boolean mLaxRef;
    private final boolean mLaxQuery;
    private final boolean mLaxPath;

    // Cached parts of {@link mKeyUrl}, following standard order within a URL.
    private final String mKeyScheme;
    private final String mProcessedKeyHost;
    private final String mKeyPort;
    private final String mProcessedKeyPath;
    private final String mKeyQuery;
    private final String mKeyRef;

    /**
     * @param keyUrl URL to be compare against.
     * @param laxHost Whether host differences (canonicalized) are allowed.
     * @param laxRef Whether #ref difference are allowed.
     * @param laxQuery Whether ?query difference are allowed.
     * @param laxPath Whether /path difference (limited to drill-down matches) are allowed.
     */
    public UrlSimilarityScorer(
            GURL keyUrl, boolean laxHost, boolean laxRef, boolean laxQuery, boolean laxPath) {
        mKeyUrl = keyUrl;
        mLaxHost = laxHost;
        mLaxRef = laxRef;
        mLaxQuery = laxQuery;
        mLaxPath = laxPath;

        mKeyScheme = mKeyUrl.getScheme();
        mProcessedKeyHost = mLaxHost ? canonicalizeHost(mKeyUrl.getHost()) : mKeyUrl.getHost();
        mKeyPort = mKeyUrl.getPort();
        // If lax /path match, ensure /path used begin and end with "/" to erase the distinction
        // between directory and files, and simplify comparison.
        mProcessedKeyPath = mLaxPath ? ensureSlashSentinel(mKeyUrl.getPath()) : mKeyUrl.getPath();
        mKeyQuery = mKeyUrl.getQuery();
        mKeyRef = mKeyUrl.getRef();
    }

    /**
     * Removes frequently seen parts of a URL host string, e.g., "www." to eliminate superficial
     * differences during matching.
     */
    @SuppressLint("DefaultLocale")
    static String canonicalizeHost(String host) {
        String buf = host.toLowerCase();
        int head = 0;
        while (true) {
            int dotPos = buf.indexOf(".", head);
            if (dotPos < 0) {
                break;
            }
            String prefix = buf.substring(head, dotPos);
            if (!sDiscardableHostPrefixes.contains(prefix)) {
                break;
            }
            head = dotPos + 1;
        }
        return buf.substring(head).toString();
    }

    /** Returns potentially modified {@param path} that starts with and ends with "/". */
    static String ensureSlashSentinel(String path) {
        boolean hasLeading = path.startsWith("/");
        boolean hasTrailing = path.endsWith("/");
        if (hasLeading && hasTrailing) return path;

        StringBuilder builder = new StringBuilder();
        if (!hasLeading) {
            builder.append('/');
        }
        builder.append(path);
        if (!hasTrailing && !path.isEmpty()) {
            builder.append('/');
        }
        return builder.toString();
    }

    /**
     * Determines whether {@param ancestorPath} is a (non-strict) ancestor of {@param path}. If so,
     * returns the latter's relative depth, with 0 being identical. Otherwise returns null. Both
     * paths must begin and end with "/".
     */
    static Integer getPathAncestralDepth(String ancestorPath, String path) {
        assert ancestorPath.startsWith("/") && ancestorPath.endsWith("/");
        assert path.startsWith("/") && path.endsWith("/");
        if (!path.startsWith(ancestorPath)) return null;

        int commonLength = ancestorPath.length();
        if (path.length() == commonLength) return 0;

        int depth = 0;
        int end = path.length();
        for (int i = commonLength; i < end; ++i) {
            if (path.charAt(i) == '/') {
                ++depth;
            }
        }
        return depth;
    }

    /** Computes the similarity score of {@param candidateUrl}. */
    public int scoreSimilarity(GURL candidateUrl) {
        if (candidateUrl.equals(mKeyUrl)) {
            return IDENTICAL;
        }

        // Scheme difference (e.g., https:// vs. http://) is MISMATCHED for web security concerns.
        // Port difference (e.g., example.com:443 vs. example.com:8000) is MISMATCHED to support
        // common usage.
        if (!candidateUrl.getScheme().contentEquals(mKeyScheme)
                || !candidateUrl.getPort().contentEquals(mKeyPort)) {
            return MISMATCHED;
        }

        String host = mLaxHost ? canonicalizeHost(candidateUrl.getHost()) : candidateUrl.getHost();
        if (!host.contentEquals(mProcessedKeyHost)) {
            return MISMATCHED;
        }

        int sim = 0;
        if (candidateUrl.getQuery().contentEquals(mKeyQuery)) {
            sim += SCORE_QUERY_MATCH;
        } else if (!mLaxQuery) {
            return MISMATCHED;
        }

        if (candidateUrl.getRef().contentEquals(mKeyRef)) {
            sim += SCORE_REF_MATCH;
        } else if (!mLaxRef) {
            return MISMATCHED;
        }

        int pathScore = PATH_MATCH_MAX_SCORE;
        if (mLaxPath) {
            String modifiedCandidatePath = ensureSlashSentinel(candidateUrl.getPath());
            Integer depth = getPathAncestralDepth(mProcessedKeyPath, modifiedCandidatePath);
            // Drill-down match: Require "key" /path to be equal to or an ancestor of "candidate"
            // /path. Decrease /path score by 1 for each level of depth, until 1 is reached.
            if (depth == null) {
                return MISMATCHED;
            }
            pathScore = Math.max(1, pathScore - depth.intValue());
        } else if (!mProcessedKeyPath.contentEquals(candidateUrl.getPath())) {
            return MISMATCHED;
        }
        sim += pathScore * SCORE_PATH_MATCH_MULTIPLIER;

        return sim;
    }

    /**
     * Returns the index of a tab in {@param tabList} whose URL attains the highest similarity
     * score, taking the first found if a tie exist.
     */
    public int findTabWithMostSimilarUrl(TabList tabList) {
        int bestIndex = TabList.INVALID_TAB_INDEX;
        int bestSim = MISMATCHED;
        int count = tabList.getCount();
        for (int i = 0; i < count; ++i) {
            int sim = scoreSimilarity(tabList.getTabAt(i).getUrl());
            if (sim != MISMATCHED && bestSim < sim) {
                bestSim = sim;
                bestIndex = i;
                // Early-exit on finding identical match.
                if (bestSim == IDENTICAL) break;
            }
        }
        return bestIndex;
    }
}
