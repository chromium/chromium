// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import android.app.Application;

/**
 * Class which fails 'NewAPI' lint check.
 */
public class NewApiTest extends Application {
    public String testTriggerNewApiCheck() {
        // This was added in API level 30.
        return getApplicationContext().getAttributionTag();
    }
}
