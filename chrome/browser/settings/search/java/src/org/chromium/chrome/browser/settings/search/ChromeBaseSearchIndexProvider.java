// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.search.BaseSearchIndexProvider;

/**
 * A specialization of {@link BaseSearchIndexProvider} for Chrome-layer fragments that rely on the
 * profile.
 *
 * <p>It implements {@link ChromeSearchIndexProvider} to allow Profile access.
 */
@NullMarked
public class ChromeBaseSearchIndexProvider extends BaseSearchIndexProvider
        implements ChromeSearchIndexProvider {

    public ChromeBaseSearchIndexProvider(String fragmentName, int xmlRes) {
        super(fragmentName, xmlRes);
    }
}
