// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CLIPBOARD_SHARED_URI;

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
    public void storeLastCopiedImageUri(@NonNull Uri uri) {
        SharedPreferencesManager.getInstance().writeString(CLIPBOARD_SHARED_URI, uri.toString());
    }

    @Override
    public @Nullable Uri getLastCopiedImageUri() {
        String uriString =
                SharedPreferencesManager.getInstance().readString(CLIPBOARD_SHARED_URI, null);
        if (TextUtils.isEmpty(uriString)) return null;

        return Uri.parse(uriString);
    }

    @Override
    public void clearLastCopiedImageUri() {
        SharedPreferencesManager.getInstance().removeKey(CLIPBOARD_SHARED_URI);
    }
}
