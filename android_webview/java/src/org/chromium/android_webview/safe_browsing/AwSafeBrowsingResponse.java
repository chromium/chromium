// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import org.chromium.build.annotations.NullMarked;

/** Container to hold the application's response to WebViewClient#onSafeBrowsingHit(). */
@NullMarked
public class AwSafeBrowsingResponse {
    private final int mAction;
    private final boolean mReporting;

    public AwSafeBrowsingResponse(int action, boolean reporting) {
        mAction = action;
        mReporting = reporting;
    }

    public int action() {
        return mAction;
    }

    public boolean reporting() {
        return mReporting;
    }
}
