// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * Java counterpart to the C++ PageContext struct. This is an opaque handle that contains the
 * serialized proto.
 */
@NullMarked
public class PageContext {
    private final byte[] mSerializedProto;

    @CalledByNative
    public PageContext(byte[] serializedProto) {
        mSerializedProto = serializedProto;
    }

    @CalledByNative
    public byte[] getSerializedProto() {
        return mSerializedProto;
    }
}
