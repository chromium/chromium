// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

/** Container to hold the application's response to WebViewClient#onSafeBrowsingHit(). */
public class AwSafeBrowsingResponse {
    private int mAction;
    private boolean mReporting;

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
