// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import org.chromium.android_webview.common.crash.CrashInfo;

import java.util.List;

/**
 * An abstract class that collects info about WebView crashes.
 */
public abstract class CrashInfoLoader {
    /**
     * Loads all crashes info from source.
     *
     * @return list of crashes info.
     */
    public abstract List<CrashInfo> loadCrashesInfo();
}
