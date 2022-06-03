// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

/**
 * This feature is no longer supported. TODO(twellington): remove this entirely Android L is no
 * longer supported.
 */
public class BookmarkAddActivity extends AppCompatActivity {
    private static final String TAG = "BookmarkAddActivity";
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "This feature is no longer supported");
        Toast.makeText(this, R.string.unsupported, Toast.LENGTH_SHORT).show();
        finish();
    }
}
