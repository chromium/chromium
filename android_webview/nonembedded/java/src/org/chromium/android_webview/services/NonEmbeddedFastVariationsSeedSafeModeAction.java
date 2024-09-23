// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.SafeModeActionIds;

/**
 * A {@link SafeModeAction} to ensure the variations seed is distributed on an app's first run. This
 * is the nonembedded-process counterpart to {@link
 * org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction}.
 */
public class NonEmbeddedFastVariationsSeedSafeModeAction implements NonEmbeddedSafeModeAction {
    // This ID should not be reused for NonEmbedded SafeMode Actions.
    private static final String ID = SafeModeActionIds.FAST_VARIATIONS_SEED;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean onActivate() {
        AwVariationsSeedFetcher.scheduleIfNeeded(/* requireFastMode= */ true);
        return true;
    }

    @Override
    public boolean onDeactivate() {
        AwVariationsSeedFetcher.cancelSafeModeSeedFetchSchedulerJob();
        return true;
    }
}
