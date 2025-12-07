// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.resources;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * Wrapper class for ResourceId so it can be called over JNI. Since ResourceId is a generated class
 * `@CalledByNative` does not work on it directly.
 */
@NullMarked
class ResourceMapper {
    @CalledByNative
    private static int[] getResourceIdList() {
        return ResourceId.getResourceIdList();
    }
}
