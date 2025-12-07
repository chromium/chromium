// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides homescreen-shortcut specific behaviour for the {@link CurrentPageVerifier}. */
@NullMarked
public class AddToHomescreenVerifier extends WebappVerifier {
    private final WebappExtras mWebappExtras;

    public AddToHomescreenVerifier(BrowserServicesIntentDataProvider intentDataProvider) {
        mWebappExtras = assertNonNull(intentDataProvider.getWebappExtras());
    }

    @Override
    protected @Nullable String getScope() {
        Origin origin = Origin.create(mWebappExtras.url);
        if (origin == null) return null;
        return origin.toString();
    }

    @Override
    protected boolean isUrlInScope(String url) {
        return UrlUtilities.sameDomainOrHost(mWebappExtras.url, url, true);
    }
}
