// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;

import org.chromium.android_webview.R;
import org.chromium.base.ContextUtils;
import org.chromium.base.JavaUtils;
import org.chromium.components.metrics.HistogramEventProtos.HistogramEventProto;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashSet;
import java.util.Set;

/**
 * Keeps a list of which histograms to upload if histograms filtering is applied.
 *
 * <p>histograms_allowlist.txt contains histograms that will be sampled at 100%. This is not done
 * for all histograms in order to preserve network usage and storage. See
 * go/clank-webview-uma#histograms-allowlist-guidance for reasons to add your histogram to it and
 * how to do it safely.
 */
public class HistogramsAllowlist {
    private final Set<Long> mHistogramNameHashes;

    private HistogramsAllowlist(Set<Long> hashes) {
        mHistogramNameHashes = hashes;
    }

    public static HistogramsAllowlist load() {
        Context appContext = ContextUtils.getApplicationContext();
        InputStream inputStream =
                appContext.getResources().openRawResource(R.raw.histograms_allowlist);
        Set<Long> hashes = new HashSet();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
            String line;
            while ((line = reader.readLine()) != null) {
                hashes.add(AwMetricsUtils.hashHistogramName(line));
            }
        } catch (IOException e) {
            JavaUtils.throwUnchecked(e);
        }

        return new HistogramsAllowlist(hashes);
    }

    public boolean contains(Long histogramNameHash) {
        return mHistogramNameHashes.contains(histogramNameHash);
    }

    public boolean contains(HistogramEventProto histogramEventProto) {
        return mHistogramNameHashes.contains(histogramEventProto.getNameHash());
    }
}
