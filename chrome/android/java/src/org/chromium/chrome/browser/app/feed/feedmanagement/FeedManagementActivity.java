// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed.feedmanagement;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.feedmanagement.FeedManagementCoordinator;

/** Activity for managing feed and webfeed settings on the new tab page. */
public class FeedManagementActivity extends SnackbarActivity {
    private static final String TAG = "FeedMActivity";
    public static final String INITIATING_STREAM_TYPE_EXTRA =
            "feed_management_initiating_stream_type_extra";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        @StreamKind
        int streamKind = getIntent().getIntExtra(INITIATING_STREAM_TYPE_EXTRA, StreamKind.UNKNOWN);

        FeedManagementCoordinator coordinator = new FeedManagementCoordinator(this, streamKind);
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
