// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Test service which communicates with a real, but non-prod endpoint.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantTestEndpointService
        implements AutofillAssistantServiceInjector.NativeServiceProvider {
    private final String mEndpointUrl;

    /**
     * Creates a test service that will communicate with a remote endpoint to retrieve scripts
     * and actions.
     *
     * @param endpointUrl The endpoint that the test service should communicate with. Must not
     *                    point to the actual prod endpoint.
     */
    AutofillAssistantTestEndpointService(String endpointUrl) {
        mEndpointUrl = endpointUrl;
    }

    /**
     * Asks the client to inject this service upon startup. Must be called prior to client startup
     * in order to take effect!
     */
    void scheduleForInjection() {
        AutofillAssistantServiceInjector.setServiceToInject(this);
    }

    @Override
    public long createNativeService(long nativeClientAndroid) {
        // Creates a native service for communicating with a test or development endpoint.
        return AutofillAssistantTestEndpointServiceJni.get().javaServiceCreate(
                this, nativeClientAndroid, mEndpointUrl);
    }

    @NativeMethods
    interface Natives {
        long javaServiceCreate(
                AutofillAssistantTestEndpointService caller, long nativeClient, String endpointUrl);
    }
}
