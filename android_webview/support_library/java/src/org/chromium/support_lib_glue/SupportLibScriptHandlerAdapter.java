// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.ScriptHandler;
import org.chromium.support_lib_boundary.ScriptHandlerBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/**
 * Adapter between ScriptHandlerBoundaryInterface and ScriptHandler.
 */
class SupportLibScriptHandlerAdapter implements ScriptHandlerBoundaryInterface {
    private ScriptHandler mScriptHandler;

    public SupportLibScriptHandlerAdapter(ScriptHandler scriptHandler) {
        mScriptHandler = scriptHandler;
    }

    @Override
    public void remove() {
        recordApiCall(ApiCall.REMOVE_DOCUMENT_START_SCRIPT);
        mScriptHandler.remove();
    }
}
