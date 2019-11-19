// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.content_public.browser.MessagePort;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;

import java.lang.reflect.InvocationHandler;

/**
 * Utility class for creating a WebMessageBoundaryInterface (this is necessary to pass a
 * WebMessage back across the boundary).
 */
public class SupportLibWebMessageAdapter implements WebMessageBoundaryInterface {
    private String mData;
    private MessagePort[] mPorts;

    /* package */ SupportLibWebMessageAdapter(String data, MessagePort[] ports) {
        mData = data;
        mPorts = ports;
    }

    @Override
    public String getData() {
        return mData;
    }

    @Override
    public /* WebMessagePort */ InvocationHandler[] getPorts() {
        return SupportLibWebMessagePortAdapter.fromMessagePorts(mPorts);
    }

    @Override
    public String[] getSupportedFeatures() {
        // getData() and getPorts() are not covered by feature flags.
        return new String[0];
    }
}
