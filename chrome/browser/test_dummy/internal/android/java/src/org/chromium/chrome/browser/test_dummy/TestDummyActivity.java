// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_dummy;

import android.app.Activity;

import org.chromium.base.CommandLine;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.modules.test_dummy.TestDummyModuleProvider;

/** Helper activity to launch test dummy module. */
@NullMarked
public class TestDummyActivity extends Activity {
    private static final String ENABLE_TEST_DUMMY_MODULE = "enable-test-dummy-module";

    @Override
    protected void onResume() {
        super.onResume();
        if (!CommandLine.getInstance().hasSwitch(ENABLE_TEST_DUMMY_MODULE)) {
            finish();
            return;
        }
        LibraryLoader.getInstance().ensureInitialized();
        if (TestDummyModuleProvider.isModuleInstalled()) {
            onModuleInstalled(true);
        } else {
            TestDummyModuleProvider.installModule(this::onModuleInstalled);
        }
    }

    private void onModuleInstalled(boolean success) {
        if (!success) throw new RuntimeException("Failed to install module");
        TestDummyModuleProvider.getTestDummyProvider().getTestDummy().launch(getIntent(), this);
    }
}
