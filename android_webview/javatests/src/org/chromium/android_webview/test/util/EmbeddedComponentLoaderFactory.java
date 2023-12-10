// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.component_updater.ComponentLoaderPolicyBridge;
import org.chromium.components.component_updater.EmbeddedComponentLoader;

import java.util.Arrays;

/**
 * A utility class to bridge to native to get list of native component loaders for
 * EmbeddedComponentLoaderTest.
 */
@JNINamespace("component_updater")
public class EmbeddedComponentLoaderFactory {
    // Shouldn't instantiate this class.
    private EmbeddedComponentLoaderFactory() {}

    public static EmbeddedComponentLoader makeEmbeddedComponentLoader() {
        return new EmbeddedComponentLoader(
                Arrays.asList(
                        EmbeddedComponentLoaderFactoryJni.get().getComponentLoaderPolicies()));
    }

    @NativeMethods
    interface Natives {
        ComponentLoaderPolicyBridge[] getComponentLoaderPolicies();
    }
}
