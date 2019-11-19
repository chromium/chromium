// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.test_dummy;

import android.support.v7.app.AppCompatActivity;

import org.chromium.base.CommandLine;
import org.chromium.chrome.modules.test_dummy.TestDummyModuleProvider;

/** Helper activity to launch test dummy module.  */
public class TestDummyActivity extends AppCompatActivity {
    private static final String ENABLE_TEST_DUMMY_MODULE = "enable-test-dummy-module";

    @Override
    protected void onResume() {
        super.onResume();
        if (!CommandLine.getInstance().hasSwitch(ENABLE_TEST_DUMMY_MODULE)) {
            finish();
            return;
        }
        if (!TestDummyModuleProvider.isModuleInstalled()) {
            TestDummyModuleProvider.installModule(this::onModuleInstalled);
            return;
        }
        onModuleInstalled(true);
    }

    private void onModuleInstalled(boolean success) {
        if (!success) throw new RuntimeException("Failed to install module");
        TestDummyModuleProvider.getTestDummyProvider().getTestDummy().launch(getIntent(), this);
    }
}
