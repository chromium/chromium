// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** UseLayout annotation is used to inform WebViewUiTestRule to launch activity with specific layout */
// TODO(yolandyan): enable Parametrized test with layout, write one test, run in multiple layouts
@Retention(RetentionPolicy.RUNTIME)
public @interface UseLayout {
    String value();
}
