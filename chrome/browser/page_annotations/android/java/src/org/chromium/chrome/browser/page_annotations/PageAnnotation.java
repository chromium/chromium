// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import androidx.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class for all page annotations.
 */
public abstract class PageAnnotation {
    private final @PageAnnotationType String mAnnotationType;

    /**
     * Enumerates the various types of {@link PageAnnotation} subclasses.
     */
    @StringDef({PageAnnotationType.UNKNOWN, PageAnnotationType.BUYABLE_PRODUCT,
            PageAnnotationType.PRODUCT_PRICE_UPDATE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PageAnnotationType {
        String UNKNOWN = "UNKNOWN";
        String BUYABLE_PRODUCT = "BUYABLE_PRODUCT";
        String PRODUCT_PRICE_UPDATE = "PRODUCT_PRICE_UPDATE";
    }

    /** Creates a new instance. */
    PageAnnotation(@PageAnnotationType String type) {
        mAnnotationType = type;
    }

    /** Gets the annotation type. */
    @PageAnnotationType
    String getType() {
        return mAnnotationType;
    }
}
