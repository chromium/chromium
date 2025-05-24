// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.jni_zero.JniStaticTestMocker;
import org.junit.rules.ExternalResource;

/** TODO(crbug.com/329069277): Delete. */
public class JniMocker extends ExternalResource {
    /** Sets the native implementation of the class using a JniStaticTestMocker */
    public void mock(JniStaticTestMocker hook, Object testInst) {
        hook.setInstanceForTesting(testInst);
    }
}
