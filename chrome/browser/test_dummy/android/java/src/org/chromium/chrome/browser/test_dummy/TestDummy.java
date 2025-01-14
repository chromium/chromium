// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_dummy;

import android.app.Activity;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;

/** Runs scenarios to test dynamic feature module functionality. */
@NullMarked
public interface TestDummy {
    /**
     * Run the test scenario encoded in the given intent.
     *
     * @param intent A test dummy intent encoding the desired test scenario.
     * @param activity The activity to display the result dialog in.
     */
    void launch(Intent intent, Activity activity);
}
