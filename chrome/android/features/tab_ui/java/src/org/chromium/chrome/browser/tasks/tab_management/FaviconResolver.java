// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/** Parent view of the up to four corner favicon images/counts. */
@FunctionalInterface
public interface FaviconResolver {
    /**
     * Asynchronously fetches the favicon for a tab.
     *
     * @param tabUrl The URL of the tab, not the url of the favicon itself.
     * @param callback Invoked with the (potentially null) favicon as a drawable.
     */
    void resolve(GURL tabUrl, Callback<Drawable> callback);
}
