// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import android.content.Context;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;

/**
 * Coordinator to set up a download interstitial which displays the progress of the most recent
 * download and provides utilities for the download once completed.
 */
public interface DownloadInterstitialCoordinator extends Destroyable {
    /** @return The view containing the download interstitial. */
    View getView();

    /**
     * Called when the download interstitial's tab is reparented.
     * @param context The context of the new parent activity.
     */
    void onTabReparented(Context context);
}
