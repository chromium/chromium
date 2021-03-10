// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.component_updater.ComponentLoaderPolicyBridge;
import org.chromium.components.component_updater.EmbeddedComponentLoader;

import java.util.ArrayList;
import java.util.List;

/**
 * A utility class to bridge to native to get list of native component loaders for
 * EmbeddedComponentLoaderTest.
 */
@JNINamespace("component_updater")
public class EmbeddedComponentLoaderFactory {
    // Shouldn't instantiate this class.
    private EmbeddedComponentLoaderFactory() {}

    public static EmbeddedComponentLoader makeEmbeddedComponentLoader() {
        long[] nativeComponentLoaders =
                EmbeddedComponentLoaderFactoryJni.get().getComponentLoaders();
        List<ComponentLoaderPolicyBridge> policyList = new ArrayList<>();
        for (long nativeLoader : nativeComponentLoaders) {
            policyList.add(new ComponentLoaderPolicyBridge(nativeLoader));
        }
        return new EmbeddedComponentLoader(policyList);
    }

    @NativeMethods
    interface Natives {
        long[] getComponentLoaders();
    }
}