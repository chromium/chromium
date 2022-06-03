// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

/**
 * A network client that returns configurable responses
 *  modified from org.chromium.chrome.browser.feed.library.mocknetworkclient.MockServerNetworkClient
 */
public class TestNetworkClient {
    public TestNetworkClient() {}

    /**
     * Set stored protobuf responses from the filePath
     *
     * @param filePath The file path of the compiled MockServer proto,
     *                 pass in null to use the default response
     */
    @VisibleForTesting
    public void setNetworkResponseFile(String filePath) {}
}
