// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides WebAPK specific behaviour for the {@link CurrentPageVerifier}. */
@NullMarked
public class WebApkVerifier extends WebappVerifier {
    private final WebappExtras mWebappExtras;

    public WebApkVerifier(BrowserServicesIntentDataProvider intentDataProvider) {
        mWebappExtras = assertNonNull(intentDataProvider.getWebappExtras());
    }

    @Override
    protected String getScope() {
        return mWebappExtras.scopeUrl;
    }

    @Override
    protected boolean isUrlInScope(String url) {
        return UrlUtilities.isUrlWithinScope(url, mWebappExtras.scopeUrl);
    }
}
