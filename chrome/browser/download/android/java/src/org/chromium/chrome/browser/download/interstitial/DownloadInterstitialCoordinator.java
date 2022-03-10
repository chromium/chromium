// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import android.view.View;

import org.chromium.base.lifetime.Destroyable;

/**
 * Coordinator to set up a download interstitial which displays the progress of the most recent
 * download and provides utilities for the download once completed.
 */
public interface DownloadInterstitialCoordinator extends Destroyable {
    /** @return The view containing the download interstitial. */
    View getView();
}
