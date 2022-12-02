// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.support_lib_boundary.JsReplyProxyBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between JsReplyProxyBoundaryInterface and JsReplyProxy.
 */
class SupportLibJsReplyProxyAdapter
        extends IsomorphicAdapter implements JsReplyProxyBoundaryInterface {
    private JsReplyProxy mReplyProxy;

    public SupportLibJsReplyProxyAdapter(JsReplyProxy replyProxy) {
        mReplyProxy = replyProxy;
    }

    @Override
    public void postMessage(String message) {
        recordApiCall(ApiCall.JS_REPLY_POST_MESSAGE);
        // TODO(crbug.com/1374142): Adopt MessagePayload in AndroidX.
        mReplyProxy.postMessage(new MessagePayload(message));
    }

    @Override
    /* package */ AwSupportLibIsomorphic getPeeredObject() {
        return mReplyProxy;
    }
}
