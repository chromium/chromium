// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

/**
 * Activity for the Creator Page.
 */
public class CreatorActivity extends SnackbarActivity {
    // CREATOR_WEB_FEED_ID is the Intent key under which the Web Feed ID is stored.
    public static final String CREATOR_WEB_FEED_ID = "CREATOR_WEB_FEED_ID";
    private WindowAndroid mWindowAndroid;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        byte[] mWebFeedId = getIntent().getByteArrayExtra(CREATOR_WEB_FEED_ID);
        super.onCreate(savedInstanceState);
        IntentRequestTracker intentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid = new ActivityWindowAndroid(this, false, intentRequestTracker);
        CreatorCoordinator coordinator = new CreatorCoordinator(this, mWebFeedId,
                getSnackbarManager(), mWindowAndroid, Profile.getLastUsedRegularProfile());
        setContentView(coordinator.getView());

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
