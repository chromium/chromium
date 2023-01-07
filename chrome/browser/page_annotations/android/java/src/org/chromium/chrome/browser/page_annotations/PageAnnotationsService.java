// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

import java.util.LinkedList;
import java.util.List;

/**
 * Responsible for fetching and aggregating {@link PageAnnotation}s from the
 * registered data sources.
 */
public class PageAnnotationsService {
    private final PageAnnotationsServiceProxy mServiceProxy;

    /**
     * Creates a new instance.
     *
     * @param serviceProxy {@link PageAnnotationsServiceProxy} instance to be used
     *                     when fetching from the server.
     */
    PageAnnotationsService(PageAnnotationsServiceProxy serviceProxy) {
        mServiceProxy = serviceProxy;
    }

    /**
     * Fetches all {@link PageAnnotation}s for the provided URL. The caller of this
     * method is expected to cast the {@link PageAnnotation} instances to their
     * specific subclasses based on their types. See
     * {@link PageAnnotationUtils#getAnnotation}.
     *
     * @param url      The URL to annotate.
     * @param callback Invoked when the fetch is complete.
     */
    public void getAnnotations(GURL url, Callback<List<PageAnnotation>> callback) {
        if (url == null || url.isEmpty()) {
            callback.onResult(new LinkedList<PageAnnotation>());
            return;
        }

        // TODO(crbug.com/1169545): Return cached annotations if possible.
        mServiceProxy.fetchAnnotations(url, (response) -> {
            // TODO(crbug.com/1169545): Cache annotations on the client.
            callback.onResult(response == null ? new LinkedList<PageAnnotation>()
                                               : response.getAnnotations());
        });
    }
}