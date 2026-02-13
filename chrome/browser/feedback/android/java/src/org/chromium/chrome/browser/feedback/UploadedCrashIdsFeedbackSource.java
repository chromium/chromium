// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.util.List;
import java.util.Map;

/** Grabs the list of uploaded crash IDs from the crash upload log. */
@NullMarked
class UploadedCrashIdsFeedbackSource extends AsyncFeedbackSourceAdapter<String> {
    private static final String TAG = "UploadedCrashIdsFS";

    @VisibleForTesting
    @Override
    public @Nullable String doInBackground(Context context) {
        CrashFileManager fileManager = new CrashFileManager(context.getCacheDir());
        List<String> uploadedIds = fileManager.readUploadedCrashIdsFromDisk();
        if (uploadedIds.isEmpty()) return null;
        return TextUtils.join(",", uploadedIds);
    }

    @Override
    public @Nullable Map<String, String> getFeedback() {
        String crashIds = getResult();
        if (crashIds == null) return null;
        return Map.of("uploaded_crash_hashes", crashIds);
    }
}
