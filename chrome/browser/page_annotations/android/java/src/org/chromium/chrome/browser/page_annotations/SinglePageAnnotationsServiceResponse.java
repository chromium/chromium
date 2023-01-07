// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import java.util.List;

/**
 * Represents the Page Annotations backend response for a single URL request.
 */
class SinglePageAnnotationsServiceResponse {
    private final List<PageAnnotation> mAnnotations;

    public SinglePageAnnotationsServiceResponse(List<PageAnnotation> annotations) {
        mAnnotations = annotations;
    }

    /**
     * List of annotations returned by the service.
     */
    public List<PageAnnotation> getAnnotations() {
        return mAnnotations;
    }
}
