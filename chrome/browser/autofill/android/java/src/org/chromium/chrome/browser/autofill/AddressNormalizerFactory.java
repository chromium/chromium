// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.autofill.AddressNormalizer;

/** Provides access to AddressNormalizer with the necessary //chrome dependencies. */
@JNINamespace("autofill")
public class AddressNormalizerFactory {
    private AddressNormalizerFactory() {}

    public static AddressNormalizer getInstance() {
        return AddressNormalizerFactoryJni.get().getInstance();
    }

    @NativeMethods
    interface Natives {
        AddressNormalizer getInstance();
    }
}
