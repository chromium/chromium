// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.TraceEvent;

import java.util.Arrays;
import java.util.List;

/** Java side of the Browser Context Store */
@JNINamespace("android_webview")
@Lifetime.Singleton
public class AwBrowserContextStore {
    /**
     * Get the context with the given name, optionally creating it if needed.
     *
     * <p>Returns null if the context does not exist and createIfNeeded is false.
     *
     * <p>Name must be non-null and valid Unicode.
     */
    public static AwBrowserContext getNamedContext(String name, boolean createIfNeeded) {
        try (TraceEvent event = TraceEvent.scoped("WebView.ProfileStore.GET_NAMED_CONTEXT")) {
            return AwBrowserContextStoreJni.get().getNamedContextJava(name, createIfNeeded);
        }
    }

    /**
     * Get the named context's relative path, without loading it in.
     *
     * <p>Will return an empty string if the context doesn't exist.
     */
    public static String getNamedContextPathForTesting(String name) {
        return AwBrowserContextStoreJni.get().getNamedContextPathForTesting(name); // IN-TEST
    }

    /**
     * Delete the named context.
     *
     * <p>Returns true if a context was deleted. Returns false if the context did not exist
     * beforehand.
     *
     * <p>Name must be non-null and valid Unicode.
     *
     * @throws IllegalArgumentException if trying to delete the default profile.
     * @throws IllegalStateException if trying to delete a profile which is in use.
     */
    public static boolean deleteNamedContext(String name)
            throws IllegalArgumentException, IllegalStateException {
        try (TraceEvent event = TraceEvent.scoped("WebView.ProfileStore.DELETE_NAMED_CONTEXT")) {
            final String defaultContextName = AwBrowserContextJni.get().getDefaultContextName();
            if (name.equals(defaultContextName)) {
                throw new IllegalArgumentException("Cannot delete the default profile");
            }
            return AwBrowserContextStoreJni.get().deleteNamedContext(name);
        }
    }

    /** List all contexts. */
    public static List<String> listAllContexts() {
        try (TraceEvent event = TraceEvent.scoped("WebView.ProfileStore.LIST_ALL_CONTEXTS")) {
            return Arrays.asList(AwBrowserContextStoreJni.get().listAllContexts());
        }
    }

    /**
     * Check whether a context with the given name exists (in memory or on disk).
     *
     * <p>Name must be non-null and valid Unicode.
     */
    public static boolean checkNamedContextExists(String name) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.ProfileStore.CHECKED_NAMED_CONTEXT_EXISTS")) {
            return AwBrowserContextStoreJni.get().checkNamedContextExists(name);
        }
    }

    @NativeMethods
    interface Natives {
        AwBrowserContext getNamedContextJava(
                @JniType("std::string") String name, boolean createIfNeeded);

        @JniType("std::string")
        String getNamedContextPathForTesting(@JniType("std::string") String name); // IN-TEST

        boolean deleteNamedContext(@JniType("std::string") String name);

        String[] listAllContexts();

        boolean checkNamedContextExists(@JniType("std::string") String name);
    }
}
