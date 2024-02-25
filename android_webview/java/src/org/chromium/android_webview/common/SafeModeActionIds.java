// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * A utility class for Variations Fast Fetch Mode to provide a common set of utilities for safemode
 * between embedded and non-embedded WebView.
 *
 * <p>Note: These IDs should not be reused in other locations than the owning SafeModeAction.
 */
public class SafeModeActionIds {
    public static final String DELETE_VARIATIONS_SEED = "delete_variations_seed";
    public static final String FAST_VARIATIONS_SEED = "fast_variations_seed";
    public static final String NOOP = "noop";
    public static final String DISABLE_ANDROID_AUTOFILL = "disable_android_autofill";
    public static final String DISABLE_ORIGIN_TRIALS = "disable_origin_trials";
    public static final String DISABLE_AW_SAFE_BROWSING = "disable_safe_browsing";
    public static final String RESET_COMPONENT_UPDATER = "reset_component_updater";
}
