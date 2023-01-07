// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webview_shell;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.webkit.WebChromeClient;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;

/**
 * This class serves as a custom contract for selecting multiple files during the
 * select file action.
 */
public class MultiFileSelector extends ActivityResultContract<Void, Uri[]> {
    private WebChromeClient.FileChooserParams mFileParams;

    @NonNull
    @Override
    public Intent createIntent(@NonNull Context context, Void param) {
        assert mFileParams != null;
        return mFileParams.createIntent();
    }

    @Override
    public Uri[] parseResult(int resultCode, @Nullable Intent result) {
        if (resultCode != Activity.RESULT_OK || result == null) {
            return null;
        }
        // For multiple file selection
        ClipData data = result.getClipData();
        if (data != null) {
            ArrayList<Uri> uris = new ArrayList<>();
            for (int i = 0; i < data.getItemCount(); i++) {
                ClipData.Item item = data.getItemAt(i);
                Uri uri = item.getUri();
                uris.add(uri);
            }
            return uris.toArray(new Uri[uris.size()]);
        } else { // For single file selection
            Uri uriData = result.getData();
            if (uriData != null) {
                return new Uri[] {uriData};
            } else {
                return null;
            }
        }
    }

    public void setFileChooserParams(WebChromeClient.FileChooserParams params) {
        mFileParams = params;
    }
}
