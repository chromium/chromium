// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.version_info;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Bridge between native and VersionConstants.java. */
@NullMarked
public class VersionConstantsBridge {
    @CalledByNative
    public static int getChannel() {
        return VersionConstants.CHANNEL;
    }
}
