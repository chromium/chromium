// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;

/**
 * This interface is used when the AwContentsClient offers a JavaScript modal prompt dialog to
 * enable the client to handle the dialog in their own way. AwContentsClient will offer an object
 * that implements this interface to the client and when the client has handled the dialog, it must
 * either callback with confirm() or cancel() to allow processing to continue.
 */
@NullMarked
public interface JsPromptResultReceiver {
    void confirm(String result);

    void cancel();
}
