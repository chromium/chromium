// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

/**
 * Activity for the Creator Page.
 */
public class CreatorActivity extends SnackbarActivity {
    // CREATOR_WEB_FEED_ID is the Intent key under which the Web Feed ID is stored.
    public static final String CREATOR_WEB_FEED_ID = "CREATOR_WEB_FEED_ID";
    public static final String CREATOR_TITLE = "CREATOR_TITLE";
    public static final String CREATOR_URL = "CREATOR_URL";
    private ActivityWindowAndroid mWindowAndroid;
    private BottomSheetController mBottomSheetController;
    private ViewGroup mBottomSheetContainer;
    private CreatorActionDelegateImpl mCreatorActionDelegate;
    private ScrimCoordinator mScrim;
    private EphemeralTabCoordinator mEphemeralTabCoordinator;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        byte[] mWebFeedId = getIntent().getByteArrayExtra(CREATOR_WEB_FEED_ID);
        String mTitle = getIntent().getStringExtra(CREATOR_TITLE);
        String mUrl = getIntent().getStringExtra(CREATOR_URL);

        super.onCreate(savedInstanceState);
        IntentRequestTracker intentRequestTracker = IntentRequestTracker.createFromActivity(this);
        mWindowAndroid = new ActivityWindowAndroid(this, false, intentRequestTracker);
        CreatorCoordinator coordinator =
                new CreatorCoordinator(this, mWebFeedId, getSnackbarManager(), mWindowAndroid,
                        Profile.getLastUsedRegularProfile(), mTitle, mUrl);

        mCreatorActionDelegate = new CreatorActionDelegateImpl();
        coordinator.initFeedStream(
                mCreatorActionDelegate, HelpAndFeedbackLauncherImpl.getInstance());

        setContentView(coordinator.getView());
        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setTitle("");
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
