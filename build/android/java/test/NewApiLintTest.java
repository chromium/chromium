// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import android.app.Application;

/** Class which fails 'NewAPI' lint check. */
public class NewApiLintTest extends Application {
    public String testTriggerNewApiCheck() {
        // This was added in API level 30.
        return getApplicationContext().getAttributionTag();
    }
}
