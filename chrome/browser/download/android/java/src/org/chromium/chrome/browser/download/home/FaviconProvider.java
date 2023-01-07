// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

/** Responsible for providing favicon for a given URL. */
public interface FaviconProvider {
    /**
     * Fetches favicon for the given URL.
     * @param url The associated URL.
     * @param faviconSizePx The desired size of the favicon in pixels.
     * @param callback The callback to be run after the favicon is fetched.
     */
    void getFavicon(final String url, int faviconSizePx, Callback<Bitmap> callback);
}
