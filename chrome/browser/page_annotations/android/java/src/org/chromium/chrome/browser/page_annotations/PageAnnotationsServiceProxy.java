// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * Contains the business logic to call the Page Annotations backend over HTTPS.
 * Deprecated in favor of OptimizationGuide
 */
public class PageAnnotationsServiceProxy {
    private final Profile mProfile;

    /**
     * Creates a new proxy instance.
     *
     * @param profile Profile to use for auth.
     */
    PageAnnotationsServiceProxy(Profile profile) {
        mProfile = profile;
    }

    /**
     * Makes an HTTPS call to the backend and returns the service response through
     * the provided callback.
     *
     * @param url      The URL to annotate.
     * @param callback {@link Callback} to invoke once the request is complete.
     */
    public void fetchAnnotations(
            GURL url, Callback<SinglePageAnnotationsServiceResponse> callback) {
        callback.onResult(null);
        return;
    }
}
