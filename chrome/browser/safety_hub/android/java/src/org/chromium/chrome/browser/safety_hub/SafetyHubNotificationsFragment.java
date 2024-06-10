// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

/**
 * Safety Hub subpage that displays a list of all to be reviewed notifications alongside their
 * supported actions.
 */
public class SafetyHubNotificationsFragment extends SafetyHubSubpageFragment {

    @Override
    protected void updatePreferenceList() {
        // TODO(crbug.com/324562205): Update the notifications list.
    }

    @Override
    protected int getTitleId() {
        return R.string.safety_hub_notifications_page_title;
    }

    @Override
    protected int getHeaderId() {
        return R.string.safety_hub_notifications_page_header;
    }

    @Override
    protected void onBottomButtonClicked() {
        // TODO(crbug.com/324562205): React to the 'Got it' button being clicked.
    }
}
