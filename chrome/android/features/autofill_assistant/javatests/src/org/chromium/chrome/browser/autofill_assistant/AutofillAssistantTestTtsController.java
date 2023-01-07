// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.AutofillAssistantDependencyInjector;

/**
 * Test TTS controller for use in integration tests.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantTestTtsController
        implements AutofillAssistantDependencyInjector.NativeTtsControllerProvider {
    /** Represents a single speak request. */
    static class SpeakRequest {
        SpeakRequest(String message, String locale) {
            mMessage = message;
            mLocale = locale;
        }
        public final String mMessage;
        public final String mLocale;
    }

    private final Callback<SpeakRequest> mOnSpeakRequestCallback;
    private final Runnable mOnStopRequestCallback;
    private long mNativeTtsController;

    AutofillAssistantTestTtsController(
            Callback<SpeakRequest> onSpeakRequestCallback, Runnable onStopRequestCallback) {
        mOnSpeakRequestCallback = onSpeakRequestCallback;
        mOnStopRequestCallback = onStopRequestCallback;
    }

    /**
     * Asks the client to inject this service request sender upon startup. Must be called prior to
     * trigger script startup in order to take effect!
     */
    void scheduleForInjection() {
        AutofillAssistantDependencyInjector.setTtsControllerToInject(this);
    }

    @Override
    public long createNativeTtsController() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point native will take ownership of the wrapper.
        mNativeTtsController = AutofillAssistantTestTtsControllerJni.get().createNative(this);

        // Hold on to the created pointer in order to be able to keep communicating with the native
        // instance.
        return mNativeTtsController;
    }

    @CalledByNative
    void speak(String message, String locale) {
        mOnSpeakRequestCallback.onResult(new SpeakRequest(message, locale));
    }

    @CalledByNative
    void stop() {
        mOnStopRequestCallback.run();
    }

    /**
     * Mimics receiving a content::TtsEventType for the current utterance.
     *
     * @param eventType An integer representing a valid content::TtsEventType value.
     */
    void simulateTtsEvent(int eventType) {
        AutofillAssistantTestTtsControllerJni.get().simulateTtsEvent(
                mNativeTtsController, AutofillAssistantTestTtsController.this, eventType);
    }

    @NativeMethods
    interface Natives {
        long createNative(AutofillAssistantTestTtsController caller);
        void simulateTtsEvent(long nativeTtsControllerAndroid,
                AutofillAssistantTestTtsController caller, int eventType);
    }
}
