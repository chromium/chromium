// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;

/** An activity that serves as an entry point for selecting Chrome items, like tabs. */
@NullMarked
public class ChromeItemPickerActivity extends SnackbarActivity {
    private static final String TAG = "ChromeItemPickerActivity";

    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        setContentView(R.layout.chrome_item_picker_activity);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
