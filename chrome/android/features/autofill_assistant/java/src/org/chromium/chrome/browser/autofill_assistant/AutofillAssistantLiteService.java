// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.content_public.browser.WebContents;

/**
 * Starts a native lite service and waits for script completion.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantLiteService
        implements AutofillAssistantServiceInjector.NativeServiceProvider {
    interface Delegate {
        /** The lite script has finished with {@code state}. */
        void onFinished(@LiteScriptFinishedState int state);
        /**
         * The lite script has started and is now running. In the first stage of the script,
         * @code{uiShown} is expected to be false, in the second stage, it is expected to be true.
         */
        void onScriptRunning(boolean uiShown);
    }
    private final WebContents mWebContents;
    private final String mTriggerScriptPath;
    private Delegate mDelegate;

    AutofillAssistantLiteService(
            WebContents webContents, String triggerScriptPath, Delegate delegate) {
        mWebContents = webContents;
        mTriggerScriptPath = triggerScriptPath;
        mDelegate = delegate;
    }

    @Override
    public long createNativeService() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point the native controller will take ownership of the wrapper.
        return AutofillAssistantLiteServiceJni.get().createLiteService(
                this, mWebContents, mTriggerScriptPath);
    }

    @CalledByNative
    private void onFinished(@LiteScriptFinishedState int state) {
        if (mDelegate != null) {
            mDelegate.onFinished(state);
            // Ignore subsequent notifications.
            mDelegate = null;
        }
    }

    @CalledByNative
    private void onScriptRunning(boolean uiShown) {
        if (mDelegate != null) {
            mDelegate.onScriptRunning(uiShown);
        }
    }

    @NativeMethods
    interface Natives {
        long createLiteService(AutofillAssistantLiteService service, WebContents webContents,
                String triggerScriptPath);
    }
}
