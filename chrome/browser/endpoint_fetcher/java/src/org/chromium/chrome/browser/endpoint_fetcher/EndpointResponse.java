// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.endpoint_fetcher;

import org.jni_zero.CalledByNative;

/** Encapsulates the response from the {@Link EndpointFetcher} */
public class EndpointResponse {
    private final String mResponseString;

    /**
     * Create the EndpointResponse
     * @param responseString the response string acquired from the endpoint
     */
    public EndpointResponse(String responseString) {
        mResponseString = responseString;
    }

    /** Response string acquired from calling an endpoint */
    public String getResponseString() {
        return mResponseString;
    }

    @CalledByNative
    private static EndpointResponse createEndpointResponse(String response) {
        return new EndpointResponse(response);
    }
}
