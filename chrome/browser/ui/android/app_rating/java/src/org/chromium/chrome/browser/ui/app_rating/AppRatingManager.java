// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;

/** Interface for managing interactions with the App Rating API. */
@NullMarked
public interface AppRatingManager {
    /**
     * Requests the review flow and launches it if successfully requested.
     *
     * @param activity The Activity context required to show the review dialog.
     * @param onComplete Callback invoked when the flow is finished or if it fails.
     */
    void requestAndShowReviewFlow(Activity activity, Runnable onComplete);
}
