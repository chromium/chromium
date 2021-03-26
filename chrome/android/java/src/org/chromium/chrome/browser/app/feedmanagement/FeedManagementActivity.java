// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feedmanagement;

import android.os.Bundle;

import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementCoordinator;

/**
 * Activity for managing feed and webfeed settings on the new tab page.
 */
public class FeedManagementActivity extends SnackbarActivity {
    private static final String TAG = "FeedManagementActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FeedManagementCoordinator coordinator = new FeedManagementCoordinator(this);
        setContentView(coordinator.getView());
    }
}
