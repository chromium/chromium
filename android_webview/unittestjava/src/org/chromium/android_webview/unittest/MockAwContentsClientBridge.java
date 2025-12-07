// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.unittest;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.android_webview.AwContentsClientBridge;
import org.chromium.android_webview.ClientCertLookupTable;

/** Used by android_webview/browser/aw_contents_client_bridge_unittest.cc */
class MockAwContentsClientBridge extends AwContentsClientBridge {

    private int mId;
    private String[] mKeyTypes;

    public MockAwContentsClientBridge() {
        super(new ClientCertLookupTable());
    }

    @Override
    protected void selectClientCertificate(
            final int id,
            final String[] keyTypes,
            byte[][] encodedPrincipals,
            final String host,
            final int port) {
        mId = id;
        mKeyTypes = keyTypes;
    }

    @CalledByNative
    private static MockAwContentsClientBridge getAwContentsClientBridge() {
        return new MockAwContentsClientBridge();
    }

    @CalledByNative
    private @JniType("std::vector<std::string>") String[] getKeyTypes() {
        return mKeyTypes;
    }

    @CalledByNative
    private int getRequestId() {
        return mId;
    }

    @CalledByNative
    private byte[][] createTestCertChain() {
        return new byte[][] {{1}};
    }
}
