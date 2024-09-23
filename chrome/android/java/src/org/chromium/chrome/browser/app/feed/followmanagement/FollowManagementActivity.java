// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed.followmanagement;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.feed.followmanagement.FollowManagementCoordinator;

/** Activity for managing feed and webfeed settings on the new tab page. */
public class FollowManagementActivity extends SnackbarActivity {
    private static final String TAG = "FollowMActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "FollowManagementActivity.onCreate");

        FollowManagementCoordinator coordinator = new FollowManagementCoordinator(this);
        setContentView(coordinator.getView());

        // Set up the toolbar and back button.
        Toolbar toolbar = findViewById(R.id.action_bar);
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
}
