// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feedmanagement;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.app.followmanagement.FollowManagementActivity;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementCoordinator;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementMediator;

/**
 * Activity for managing feed and webfeed settings on the new tab page.
 */
public class FeedManagementActivity
        extends SnackbarActivity implements FeedManagementMediator.FollowManagementLauncher {
    private static final String TAG = "FeedMActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FeedManagementCoordinator coordinator = new FeedManagementCoordinator(this, this);
        setContentView(coordinator.getView());
    }

    // FollowManagementLauncher method.
    @Override
    public void launch(Context context) {
        try {
            // Launch a new activity for the following management page.
            Intent intent = new Intent(context, FollowManagementActivity.class);
            Log.d(TAG, "Launching follow management activity.");
            context.startActivity(intent);
        } catch (Exception e) {
            Log.d(TAG, "Failed to launch activity " + e);
        }
    }
}
