// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import org.jni_zero.NativeMethods;

/** Helper class to initialize ResourceIdMapper for MessageDispatcherBridge. */
public class MessagesResourceMapperInitializer {
    /** Calls native method to initialize ResourceIdMapper for MessageDispatcherBridge. */
    public static void init() {
        MessagesResourceMapperInitializerJni.get().init();
    }

    @NativeMethods
    interface Natives {
        void init();
    }
}
