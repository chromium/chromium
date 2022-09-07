// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessageCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessagePortBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

/**
 * Adapter working on top of WebMessageCallbackBoundaryInterface to provide methods using boundary
 * interfaces instead of InvocationHandlers as parameters.
 */
class SupportLibWebMessageCallbackAdapter {
    WebMessageCallbackBoundaryInterface mImpl;

    SupportLibWebMessageCallbackAdapter(WebMessageCallbackBoundaryInterface impl) {
        mImpl = impl;
    }

    public void onMessage(
            WebMessagePortBoundaryInterface port, WebMessageBoundaryInterface message) {
        // Ensure WebMessageCallbackCompat.onMessage() is supported by the support library before
        // calling it.
        String[] supportedFeatures = mImpl.getSupportedFeatures();
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                    supportedFeatures, Features.WEB_MESSAGE_CALLBACK_ON_MESSAGE)) {
            mImpl.onMessage(BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(port),
                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(message));
        }
    }
}
