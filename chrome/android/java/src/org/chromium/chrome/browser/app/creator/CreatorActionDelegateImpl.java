// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** Implements some actions for the Feed */
public class CreatorActionDelegateImpl implements FeedActionDelegate {
    private static final String TAG = "Cormorant";

    public CreatorActionDelegateImpl() {}
    @Override
    public void downloadPage(String url) {
        // TODO(crbug/1385185) Add glue code to access download page action.
    }

    @Override
    public void openSuggestionUrl(int disposition, LoadUrlParams params, boolean inGroup,
            Runnable onPageLoaded, Callback<VisitResult> onVisitComplete) {
        // Back of card actions
        if (disposition == WindowOpenDisposition.NEW_FOREGROUND_TAB
                || disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD) {
            boolean offTheRecord = (disposition == WindowOpenDisposition.OFF_THE_RECORD);
            new TabDelegate(offTheRecord).createNewTab(params, TabLaunchType.FROM_LINK, null);
            return;
        }

        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
        Log.w(TAG, "OpenSuggestionUrl: Unhandled disposition " + disposition);
    }

    @Override
    public void openUrl(int disposition, LoadUrlParams params) {}

    @Override
    public void openHelpPage() {
        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
    }

    @Override
    public void addToReadingList(String title, String url) {
        // TODO(crbug/1385187) create glue code for accessing bookmark model.
    }

    @Override
    public void openCrow(String url) {
        // Unused; feature is deprecated and does not launch from the feed.
    }

    @Override
    public void onContentsChanged() {
        // Not sure if this needs to be implemented.
    }

    @Override
    public void onStreamCreated() {
        // Not sure if this needs to be implemented.
    }

    @Override
    public void showSignInActivity() {
        // TODO(crbug.com/1395449)
    }
}
