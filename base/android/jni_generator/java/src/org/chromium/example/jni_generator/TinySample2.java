// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.jni_generator;

import org.chromium.base.annotations.NativeMethods;

class TinySample2 {
    @NativeMethods()
    interface Natives {
        void test();
    }
}
