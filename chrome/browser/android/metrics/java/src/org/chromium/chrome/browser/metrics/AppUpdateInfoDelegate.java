// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

import org.chromium.build.annotations.NullMarked;

/** Base class for defining methods where different behavior is required by downstream targets. */
@NullMarked
public class AppUpdateInfoDelegate {
    public void emitToHistogram() {}
}
