// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import android.app.Application;

/**
 * A class with methods that are meant to trigger lint warnings. If it does not trigger these
 * expected warnings, then the build will fail. This prevents lint regressions where lint warnings
 * are accidentally disabled.
 */
public class LintTest extends Application {
    public String testTriggerDefaultLocaleCheck(int any) {
        // String format with an integer requires a Locale since it may be formatted differently.
        return String.format("Test %d", any);
    }

    public String testTriggerNewApiCheck() {
        // This was added in API level 30.
        return getApplicationContext().getAttributionTag();
    }
}
