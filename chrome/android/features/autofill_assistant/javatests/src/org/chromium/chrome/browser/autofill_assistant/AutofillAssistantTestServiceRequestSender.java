// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import com.google.protobuf.GeneratedMessageLite;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.List;

/**
 * Test service request sender which serves preconfigured answers to server requests.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantTestServiceRequestSender
        implements AutofillAssistantServiceInjector.NativeServiceRequestSenderProvider {
    static class Request {
        Request(String url, byte[] request) {
            mUrl = url;
            mRequest = request;
        }
        String mUrl;
        byte[] mRequest;
    }

    // TODO(arbesser): make this field accessible to callers.
    private final List<Request> mReceivedRequests = new ArrayList();
    private long mNativeServiceRequestSender;

    int mNextHttpStatus;
    GeneratedMessageLite mNextResponse;

    AutofillAssistantTestServiceRequestSender() {}

    /**
     * Asks the client to inject this service request sender upon startup. Must be called prior to
     * trigger script startup in order to take effect!
     */
    void scheduleForInjection() {
        AutofillAssistantServiceInjector.setServiceRequestSenderToInject(this);
    }

    /**
     * Sets the response that will be returned for the next (and only the next) call to {@code
     * SendRequest}.
     */
    void setNextResponse(int httpStatus, GeneratedMessageLite response) {
        mNextHttpStatus = httpStatus;
        mNextResponse = response;
    }

    @Override
    public long createNativeServiceRequestSender() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point native will take ownership of the wrapper.
        mNativeServiceRequestSender =
                AutofillAssistantTestServiceRequestSenderJni.get().createNative(this);

        // Hold on to the created pointer in order to be able to keep communicating with the native
        // instance.
        return mNativeServiceRequestSender;
    }

    @CalledByNative
    void sendRequest(String url, byte[] request) {
        assert mNextResponse != null;

        mReceivedRequests.add(new Request(url, request));
        AutofillAssistantTestServiceRequestSenderJni.get().onResponse(mNativeServiceRequestSender,
                AutofillAssistantTestServiceRequestSender.this, mNextHttpStatus,
                mNextResponse.toByteArray());

        mNextResponse = null;
        mNextHttpStatus = -1;
    }

    @NativeMethods
    interface Natives {
        long createNative(AutofillAssistantTestServiceRequestSender caller);
        void onResponse(long nativeJavaServiceRequestSender,
                AutofillAssistantTestServiceRequestSender caller, int httpStatus, byte[] response);
    }
}
