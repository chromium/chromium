// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed.feedmanagement;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.app.feed.followmanagement.FollowManagementActivity;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementCoordinator;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementMediator;

/** Activity for managing feed and webfeed settings on the new tab page. */
public class FeedManagementActivity extends SnackbarActivity
        implements FeedManagementMediator.FollowManagementLauncher {
    private static final String TAG = "FeedMActivity";
    public static final String INITIATING_STREAM_TYPE_EXTRA =
            "feed_management_initiating_stream_type_extra";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        @StreamKind
        int streamKind = getIntent().getIntExtra(INITIATING_STREAM_TYPE_EXTRA, StreamKind.UNKNOWN);

        FeedManagementCoordinator coordinator =
                new FeedManagementCoordinator(this, this, streamKind);
        setContentView(coordinator.getView());

        // Set up the toolbar and back button.
        Toolbar toolbar = (Toolbar) findViewById(R.id.action_bar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    // FollowManagementLauncher method.
    @Override
    public void launchFollowManagement(Context context) {
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
