// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.variations;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.Log;

import java.io.File;

/** A {@link SafeModeAction} to delete the variations seed. */
@Lifetime.Singleton
public class VariationsSeedSafeModeAction implements SafeModeAction {
    private static final String TAG = "WebViewSafeMode";

    // This ID should not be changed or reused.
    private static final String ID = SafeModeActionIds.DELETE_VARIATIONS_SEED;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        boolean success = true;
        // Try deleting each file even if a previous step failed, but indicate the overall success
        // of all steps.
        success &= deleteIfExists(VariationsUtils.getSeedFile());
        success &= deleteIfExists(VariationsUtils.getNewSeedFile());
        success &= deleteIfExists(VariationsUtils.getStampFile());
        return success;
    }

    private static boolean deleteIfExists(File file) {
        if (!file.exists()) {
            Log.i(TAG, "File does not exist (skipping): %s", file);
            return true;
        }
        if (file.delete()) {
            Log.i(TAG, "Successfully deleted %s", file);
            return true;
        } else {
            Log.e(TAG, "Failed to delete %s", file);
            return false;
        }
    }
}
