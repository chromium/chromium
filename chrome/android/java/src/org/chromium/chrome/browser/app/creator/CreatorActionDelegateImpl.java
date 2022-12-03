// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.content_public.browser.LoadUrlParams;

/** Implements some actions for the Feed */
public class CreatorActionDelegateImpl implements FeedActionDelegate {
    public CreatorActionDelegateImpl() {}
    @Override
    public void downloadPage(String url) {
        // TODO(crbug/1385185) Add glue code to access download page action.
    }

    @Override
    public void openSuggestionUrl(int disposition, LoadUrlParams params, boolean inGroup,
            Runnable onPageLoaded, Callback<VisitResult> onVisitComplete) {
        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
    }

    @Override
    public void openUrl(int disposition, LoadUrlParams params) {
        // TODO(crbug.com/1395448) open in ephemeral tab or thin web view.
    }

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
        // Not sure if this needs to be implemented.
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