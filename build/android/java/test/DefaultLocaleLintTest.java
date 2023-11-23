// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test;

import android.app.Application;

/** Class which fails 'DefaultLocale' lint check. */
public class DefaultLocaleLintTest extends Application {
    public String testTriggerDefaultLocaleCheck(int any) {
        // String format with an integer requires a Locale since it may be formatted differently.
        return String.format("Test %d", any);
    }
}
