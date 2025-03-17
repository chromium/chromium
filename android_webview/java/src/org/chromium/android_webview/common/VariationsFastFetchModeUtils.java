// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.TimeUnit;

/**
 * A utility class for Variations Fast Fetch Mode to provide
 * a common set of utilities for safemode between
 * embedded and non-embedded WebView.
 */
@NullMarked
public class VariationsFastFetchModeUtils {
    public static final String URI_PATH = "/safe-mode-seed-fetch-results";
    public static final long MAX_ALLOWABLE_SEED_AGE_MS = TimeUnit.MINUTES.toMillis(15);
}
