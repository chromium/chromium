// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
class JsResultHandler implements JsResultReceiver, JsPromptResultReceiver {
    private @Nullable AwContentsClientBridge mBridge;
    private final int mId;

    JsResultHandler(AwContentsClientBridge bridge, int id) {
        mBridge = bridge;
        mId = id;
    }

    @Override
    public void confirm() {
        confirm(null);
    }

    @Override
    public void confirm(final @Nullable String promptResult) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mBridge != null) mBridge.confirmJsResult(mId, promptResult);
                    mBridge = null;
                });
    }

    @Override
    public void cancel() {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mBridge != null) mBridge.cancelJsResult(mId);
                    mBridge = null;
                });
    }
}
