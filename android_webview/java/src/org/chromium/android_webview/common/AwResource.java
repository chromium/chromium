// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.res.Resources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * A class that defines a set of resource IDs and functionality to resolve
 * those IDs to concrete resources.
 */
@JNINamespace("android_webview::AwResource")
public class AwResource {
    // Array resource ID for the configuration of platform specific key-systems, must be initialized
    // by the embedder.
    private static int sStringArrayConfigKeySystemUUIDMapping;

    // The embedder should inject a Resources object that will be used
    // to resolve Resource IDs into the actual resources.
    private static Resources sResources;

    public static void setResources(Resources resources) {
        sResources = resources;
    }

    public static void setConfigKeySystemUuidMapping(int config) {
        sStringArrayConfigKeySystemUUIDMapping = config;
    }

    @CalledByNative
    private static String[] getConfigKeySystemUuidMapping() {
        // No need to cache, since this should be called only once.
        return sResources.getStringArray(sStringArrayConfigKeySystemUUIDMapping);
    }
}
