// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import android.app.Application;
import android.content.Context;

public class LintTest extends Application {
    public static String testTriggerInlinedApiCheck() {
        // This was added in API level 30.
        return Context.CONNECTIVITY_DIAGNOSTICS_SERVICE;
    }

    public String testTriggerNewApiCheck() {
        // This was added in API level 30.
        return getApplicationContext().getAttributionTag();
    }
}
