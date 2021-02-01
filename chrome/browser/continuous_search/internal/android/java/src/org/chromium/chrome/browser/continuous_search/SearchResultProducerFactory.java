// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Factory for creating a {@link SearchResultProducer}.
 */
public class SearchResultProducerFactory {
    private static SearchResultProducerFactoryImpl sFactoryImpl;

    /**
     * Interface to allow overriding of {@link sFactoryImpl}.
     */
    public interface SearchResultProducerFactoryImpl {
        SearchResultProducer create(Tab tab, SearchResultListener listener);
    }

    static SearchResultProducer create(Tab tab, SearchResultListener listener) {
        if (sFactoryImpl != null) {
            return sFactoryImpl.create(tab, listener);
        }

        return new SearchResultExtractorProducer(tab, listener);
    }

    @VisibleForTesting
    static void overrideFactory(SearchResultProducerFactoryImpl factory) {
        sFactoryImpl = factory;
    }
}
