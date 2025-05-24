// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides homescreen-shortcut specific behaviour for the {@link CurrentPageVerifier}. */
public class AddToHomescreenVerifier extends WebappVerifier {
    private final WebappExtras mWebappExtras;

    public AddToHomescreenVerifier(BrowserServicesIntentDataProvider intentDataProvider) {
        mWebappExtras = intentDataProvider.getWebappExtras();
        assert mWebappExtras != null;
    }

    @Override
    protected String getScope() {
        Origin origin = Origin.create(mWebappExtras.url);
        if (origin == null) return null;
        return origin.toString();
    }

    @Override
    protected boolean isUrlInScope(String url) {
        return UrlUtilities.sameDomainOrHost(mWebappExtras.url, url, true);
    }
}
