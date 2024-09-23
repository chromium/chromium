// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Heuristically scores the similarity between a "key" URL with one or more "candidate" URLs. The
 * main use is to find the best match among "candidate" URLs from a TabList.
 */
public class UrlSimilarityScorer {

    /** Return value for findTabWithMostSimilarUrl(). */
    static class MatchResult {
        public final int index;
        public final int score;

        public MatchResult(int index, int score) {
            this.index = index;
            this.score = score;
        }
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // Information on the tile clicked by the user. The values must be consistent with
    // MvtReselectUrlMatchResult in enums.xml.
    @IntDef({
        MvtReselectUrlMatchResult.NONE,
        MvtReselectUrlMatchResult.EXACT,
        MvtReselectUrlMatchResult.PARTIAL,
        MvtReselectUrlMatchResult.NUM_ENTRIES
    })
    @interface MvtReselectUrlMatchResult {
        int NONE = 0;
        int EXACT = 1;
        int PARTIAL = 2;
        int NUM_ENTRIES = 3;
    }

    private static final String HISTOGRAM_MATCH_PREFIX = "NewTabPage.MostVisited.ReselectMatch.";
    private static final String HISTOGRAM_ARM_LAX_UP_TO_PATH = "LaxUpToPath";
    private static final String HISTOGRAM_ARM_LAX_UP_TO_QUERY = "LaxUpToQuery";
    private static final String HISTOGRAM_ARM_LAX_UP_TO_REF = "LaxUpToRef";
    private static final String HISTOGRAM_ARM_STRICT = "Strict";

    // Value for candidate URL rejection, set to negative since match scores are non-negative.
    public static final int MISMATCHED = -1;
    // Exact matches beat all, and so is assigned maximal value.
    public static final int EXACT = Integer.MAX_VALUE;

    public static final int PATH_MATCH_MAX_SCORE = 99;
    public static final int SCORE_PATH_MATCH_MULTIPLIER = 10;
    public static final int SCORE_QUERY_MATCH = 2;
    public static final int SCORE_REF_MATCH = 1;

    // These are the same ones used in VisitSegmentDatabase::ComputeSegmentName().
    private static Set<String> sDiscardableHostPrefixes =
            new HashSet<>(Arrays.asList("www", "m", "mobile", "touch"));

    private final GURL mKeyUrl;
    // Match parameters, ordered so intended usage would specify "true" before "false".
    private final boolean mLaxSchemeHost;
    private final boolean mLaxRef;
    private final boolean mLaxQuery;
    private final boolean mLaxPath;

    // Cached parts of {@link mKeyUrl}, following standard order within a URL.
    private final Set<String> mCompatibleSchemes;
    private final String mProcessedKeyHost;
    private final String mKeyPort;
    private final String mProcessedKeyPath;
    private final String mKeyQuery;
    private final String mKeyRef;

    /**
     * @param keyUrl URL to be compare against.
     * @param laxSchemeHost Whether scheme differences (identical or http -> https) and / or host
     *     differences (canonicalized) are allowed.
     * @param laxRef Whether #ref difference are allowed.
     * @param laxQuery Whether ?query difference are allowed.
     * @param laxPath Whether /path difference (limited to drill-down matches) are allowed.
     */
    public UrlSimilarityScorer(
            GURL keyUrl, boolean laxSchemeHost, boolean laxRef, boolean laxQuery, boolean laxPath) {
        mKeyUrl = keyUrl;
        mLaxSchemeHost = laxSchemeHost;
        mLaxRef = laxRef;
        mLaxQuery = laxQuery;
        mLaxPath = laxPath;

        mCompatibleSchemes = new HashSet<String>();
        String keyScheme = mKeyUrl.getScheme();
        mCompatibleSchemes.add(keyScheme);
        if (laxSchemeHost && keyScheme.contentEquals(UrlConstants.HTTP_SCHEME)) {
            // Special case: Allow key scheme "http" to match candidate scheme "https", i.e.,
            // enter security boundary.
            mCompatibleSchemes.add(UrlConstants.HTTPS_SCHEME);
        }

        mProcessedKeyHost =
                mLaxSchemeHost ? canonicalizeHost(mKeyUrl.getHost()) : mKeyUrl.getHost();
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
            return EXACT;
        }

        // Port difference (e.g., example.com:443 vs. example.com:8000) is MISMATCHED to support
        // common usage. Scheme compatibility is determined by lookup, and is MISMATCHED on failure.
        if (!candidateUrl.getPort().contentEquals(mKeyPort)
                || !mCompatibleSchemes.contains(candidateUrl.getScheme())) {
            return MISMATCHED;
        }

        String host =
                mLaxSchemeHost ? canonicalizeHost(candidateUrl.getHost()) : candidateUrl.getHost();
        if (!host.contentEquals(mProcessedKeyHost)) {
            return MISMATCHED;
        }

        int score = 0;
        if (candidateUrl.getQuery().contentEquals(mKeyQuery)) {
            score += SCORE_QUERY_MATCH;
        } else if (!mLaxQuery) {
            return MISMATCHED;
        }

        if (candidateUrl.getRef().contentEquals(mKeyRef)) {
            score += SCORE_REF_MATCH;
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
        score += pathScore * SCORE_PATH_MATCH_MULTIPLIER;

        return score;
    }

    /**
     * Finds the tab in {@param tabList} whose URL attains the highest similarity score, taking the
     * first if a tie exists, and returns the result.
     */
    public MatchResult findTabWithMostSimilarUrl(TabList tabList) {
        int bestIndex = TabList.INVALID_TAB_INDEX;
        int bestScore = MISMATCHED;
        int count = tabList.getCount();
        for (int i = 0; i < count; ++i) {
            int score = scoreSimilarity(tabList.getTabAt(i).getUrl());
            if (score != MISMATCHED && bestScore < score) {
                bestScore = score;
                bestIndex = i;
                // Early-exit on finding identical match.
                if (bestScore == EXACT) break;
            }
        }
        return new MatchResult(bestIndex, bestScore);
    }

    /**
     * Returns the suffix string to compute histogram name, based on lax match configurations.
     * Returns null if the configuration is unrecognized for logging.
     */
    public @Nullable String getHistogramStrictnessSuffix() {
        if (!mLaxSchemeHost) {
            return !mLaxRef && !mLaxQuery && !mLaxPath ? HISTOGRAM_ARM_STRICT : null;
        }
        if (!mLaxRef) return null;

        if (!mLaxQuery) return mLaxPath ? null : HISTOGRAM_ARM_LAX_UP_TO_REF;

        return mLaxPath ? HISTOGRAM_ARM_LAX_UP_TO_PATH : HISTOGRAM_ARM_LAX_UP_TO_QUERY;
    }

    /**
     * Records histograms for the given {@param matchResult}, which is assumed to be generated by
     * this class instance (whose configs are used to find the right histogram suffix).
     */
    public void recordMatchResult(MatchResult matchResult) {
        String suffix = getHistogramStrictnessSuffix();
        if (TextUtils.isEmpty(suffix)) return;

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_MATCH_PREFIX + suffix,
                scoreToMvtReselectUrlMatchResult(matchResult.score),
                MvtReselectUrlMatchResult.NUM_ENTRIES);
    }

    int scoreToMvtReselectUrlMatchResult(int score) {
        if (score == MISMATCHED) return MvtReselectUrlMatchResult.NONE;

        if (score == EXACT) return MvtReselectUrlMatchResult.EXACT;

        return MvtReselectUrlMatchResult.PARTIAL;
    }
}
