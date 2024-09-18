// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.data_sharing.DataSharingSDKDelegate;

/**
 * Helper class for testing that provides functionality for setting bridge {@link
 * DataSharingSDKDelegateBridge} over JNI with test implementation of {@link
 * DataSharingSDKDelegate}.
 */
@JNINamespace("data_sharing")
public class DataSharingServiceFactoryBridge {

    @CalledByNative
    private static DataSharingSDKDelegate createJavaSDKDelegate(Profile profile) {
        DataSharingImplFactory factory =
                ServiceLoaderUtil.maybeCreate(DataSharingImplFactory.class);
        if (factory != null) {
            return factory.createSdkDelegate(profile);
        }
        return new NoOpDataSharingSDKDelegateImpl();
    }
}
