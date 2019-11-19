// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.complex_tasks.endpoint_fetcher;

import org.chromium.base.annotations.CalledByNative;

/**
 * Encapsulates the response from the {@Link EndpointFetcher}
 */
public class EndpointResponse {
    private final String mResponseString;

    /**
     * Create the EndpointResponse
     * @param responseString the response string acquired from the endpoint
     */
    public EndpointResponse(String responseString) {
        mResponseString = responseString;
    }

    /**
     * Response string acquired from calling an endpoint
     */
    public String getResponseString() {
        return mResponseString;
    }

    @CalledByNative
    private static EndpointResponse createEndpointResponse(String response) {
        return new EndpointResponse(response);
    }
}
