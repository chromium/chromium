// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/**
 * Provides access to a service which is to be injected upon client startup.
 *
 * <p> This is intended to allow tests to inject test services to be used by the native side. </p>
 */
public class AutofillAssistantServiceInjector {
    /**
     * Interface for service providers.
     */
    public interface NativeServiceProvider {
        /**
         * Returns a pointer to a native service instance, or 0 if no service was created.
         */
        long createNativeService();
    }

    /**
     * Provider to create the native service to inject. Will be automatically called upon client
     * startup.
     */
    private static NativeServiceProvider sNativeServiceProvider;

    /**
     * Sets a service provider to create a native service to inject upon client startup.
     */
    public static void setServiceToInject(NativeServiceProvider nativeServiceProvider) {
        sNativeServiceProvider = nativeServiceProvider;
    }

    /**
     * Returns the native pointer to the service to inject, or 0 if no service has been set (and the
     * default should be used).
     *
     * <p>Please note: the caller must ensure to take ownership of the returned native pointer,
     * else it will leak!</p>
     */
    public static long getServiceToInject() {
        if (sNativeServiceProvider == null) {
            return 0;
        }

        return sNativeServiceProvider.createNativeService();
    }
}
