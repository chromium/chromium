// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CLIPBOARD_SHARED_URI;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CLIPBOARD_SHARED_URI_TIMESTAMP;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.ui.base.Clipboard;

/**
 * Implementation class for {@link Clipboard.ImageFileProvider}.
 */
public class ClipboardImageFileProvider implements Clipboard.ImageFileProvider {
    @Override
    public void storeImageAndGenerateUri(
            byte[] imageData, String fileExtension, Callback<Uri> callback) {
        ShareImageFileUtils.generateTemporaryUriFromData(imageData, fileExtension, callback);
    }

    @Override
    public void storeLastCopiedImageMetadata(@NonNull ClipboardFileMetadata clipboardFileMetadata) {
        SharedPreferencesManager.getInstance().writeString(
                CLIPBOARD_SHARED_URI, clipboardFileMetadata.uri.toString());
        SharedPreferencesManager.getInstance().writeLong(
                CLIPBOARD_SHARED_URI_TIMESTAMP, clipboardFileMetadata.timestamp);
    }

    @Override
    public @Nullable ClipboardFileMetadata getLastCopiedImageMetadata() {
        String uriString =
                SharedPreferencesManager.getInstance().readString(CLIPBOARD_SHARED_URI, null);
        if (TextUtils.isEmpty(uriString)) return null;

        Uri uri = Uri.parse(uriString);
        long timestamp =
                SharedPreferencesManager.getInstance().readLong(CLIPBOARD_SHARED_URI_TIMESTAMP);

        return new ClipboardFileMetadata(uri, timestamp);
    }

    @Override
    public void clearLastCopiedImageMetadata() {
        SharedPreferencesManager.getInstance().removeKey(CLIPBOARD_SHARED_URI);
        SharedPreferencesManager.getInstance().removeKey(CLIPBOARD_SHARED_URI_TIMESTAMP);
    }
}
