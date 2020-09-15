// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Manage local cache of trendy search terms.
 */
public class TrendyTermsCache {
    private static final String TAG = "TrendyTerms";

    private static final String PREFERENCES_NAME = "trendy_terms";
    private static SharedPreferences sPref;
    @VisibleForTesting
    static final String SUPPRESSED_UNTIL_KEY = "suppressed-until";
    private static final String COUNT_KEY = "count";
    private static final String TERMS_KEY_PREFIX = "term_";

    private static TrendyTermsCache sInstance;

    @VisibleForTesting
    static SharedPreferences getSharedPreferences() {
        if (sPref == null) {
            sPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    PREFERENCES_NAME, Context.MODE_PRIVATE);
        }
        return sPref;
    }

    @VisibleForTesting
    static void setInstanceForTesting(TrendyTermsCache instance) {
        sInstance = instance;
    }

    private static TrendyTermsCache getInstance() {
        if (sInstance == null) {
            sInstance = new TrendyTermsCache();
        }
        return sInstance;
    }

    @VisibleForTesting
    TrendyTermsCache() {}

    /**
     * Try to fetch latest trendy terms. Fetching is subject to rate control.
     */
    @SuppressLint("ApplySharedPref")
    public static void maybeFetch(Profile profile) {
        if (!StartSurfaceConfiguration.TRENDY_ENABLED.getValue()) return;
        Callback<EndpointResponse> callback = result -> {
            if (result == null) return;
            try {
                StringReader reader = new StringReader(result.getResponseString());
                List<String> terms = parseRSS(reader);
                if (terms == null) return;
                saveTrendyTerms(terms);

                getSharedPreferences()
                        .edit()
                        .putLong(SUPPRESSED_UNTIL_KEY,
                                getInstance().getCurrentTime()
                                        + StartSurfaceConfiguration.TRENDY_SUCCESS_MIN_PERIOD_MS
                                                  .getValue())
                        .apply();
            } catch (IOException e) {
                Log.w(TAG, "Failed parsing trendy terms.", e);
            }
        };
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            // TODO(wychen): when changing locale, fetch new terms regardless of rate control.
            if (!getInstance().shouldFetch()) return;

            getSharedPreferences()
                    .edit()
                    .putLong(SUPPRESSED_UNTIL_KEY,
                            getInstance().getCurrentTime()
                                    + StartSurfaceConfiguration.TRENDY_FAILURE_MIN_PERIOD_MS
                                              .getValue())
                    .commit();

            // TODO(wychen): skip fetching unsupported locale.
            String url = StartSurfaceConfiguration.TRENDY_ENDPOINT.getValue()
                    + Locale.getDefault().getCountry();
            PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT,
                    () -> EndpointFetcherJni.get().nativeFetchWithNoAuth(profile, url, callback));
        });
    }

    /**
     * Return trendy terms from local cache.
     */
    public static List<String> getTrendyTerms() {
        List<String> terms = new ArrayList<>();
        int count = getSharedPreferences().getInt(COUNT_KEY, 0);
        for (int i = 0; i < count; i++) {
            String term = getSharedPreferences().getString(TERMS_KEY_PREFIX + i, "");
            terms.add(term);
        }
        return terms;
    }

    @VisibleForTesting
    long getCurrentTime() {
        return System.currentTimeMillis();
    }

    // Not static in order to be mocked.
    @VisibleForTesting
    boolean shouldFetch() {
        long now = getCurrentTime();
        long suppressedUntil = getSharedPreferences().getLong(SUPPRESSED_UNTIL_KEY, -1);
        if (now < suppressedUntil) {
            Log.d(TAG, "Skip fetching until %d (now is %d).", suppressedUntil, now);
            return false;
        }
        return true;
    }

    // TODO(wychen): use a real XML parser.
    @VisibleForTesting
    @Nullable
    static List<String> parseRSS(Reader rawReader) throws IOException {
        BufferedReader reader = new BufferedReader(rawReader);
        List<String> terms = new ArrayList<>();
        String line = reader.readLine();
        while (line != null) {
            line = line.trim();
            if (line.startsWith("<title>")) {
                String term = line.replace("<title>", "").replace("</title>", "");
                term = term.trim();
                if (!TextUtils.isEmpty(term)) {
                    terms.add(term);
                }
            }
            line = reader.readLine();
        }
        reader.close();
        if (terms.size() <= 1) return null;
        terms.remove(0);
        return terms;
    }

    @VisibleForTesting
    static void saveTrendyTerms(List<String> terms) {
        int id = 0;
        Editor editor = getSharedPreferences().edit();
        editor.putInt(COUNT_KEY, terms.size());
        for (String term : terms) {
            editor.putString(TERMS_KEY_PREFIX + id, term);
            id++;
        }
        editor.apply();
        Log.d(TAG, "Saved trendy terms: %s", terms.toString());
    }
}
