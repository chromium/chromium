// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service;

import android.app.Activity;
import android.os.Bundle;

/**
 * A sample settings activity that opens on clicking the inline suggestion icon. It does nothing.
 */
public class SettingsActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
    }
}
