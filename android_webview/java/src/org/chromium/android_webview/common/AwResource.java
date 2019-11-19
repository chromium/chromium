// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.res.Resources;
import android.util.SparseArray;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.ref.SoftReference;

/**
 * A class that defines a set of resource IDs and functionality to resolve
 * those IDs to concrete resources.
 */
@JNINamespace("android_webview::AwResource")
public class AwResource {
    // The following resource ID's must be initialized by the embedder.

    // Array resource ID for the configuration of platform specific key-systems.
    private static int sStringArrayConfigKeySystemUUIDMapping;

    // The embedder should inject a Resources object that will be used
    // to resolve Resource IDs into the actual resources.
    private static Resources sResources;

    // Loading some resources is expensive, so cache the results.
    private static SparseArray<SoftReference<String>> sResourceCache;

    public static void setResources(Resources resources) {
        sResources = resources;
        sResourceCache = new SparseArray<SoftReference<String>>();
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
