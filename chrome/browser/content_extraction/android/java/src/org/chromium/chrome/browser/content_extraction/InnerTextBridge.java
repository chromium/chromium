// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_extraction;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.RenderFrameHost;

import java.util.Optional;

public class InnerTextBridge {
    public static void getInnerText(
            RenderFrameHost webContents, Callback<Optional<String>> innerTextCallback) {
        InnerTextBridgeJni.get().getInnerText(webContents, innerTextCallback);
    }

    @NativeMethods
    public interface Natives {
        void getInnerText(
                RenderFrameHost webContents, Callback<Optional<String>> innerTextCallback);
    }
}
