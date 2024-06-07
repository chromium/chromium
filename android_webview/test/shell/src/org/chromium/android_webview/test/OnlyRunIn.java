// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Specify that a test case or test suite should only run in single or multi process mode or either
 * if the code being tested does not depend on the renderer process so the test will not benefit
 * from duplication. In this case the test will just be run in one mode for efficiency. Without this
 * annotation, {@link AwJUnit4ClassRunner} defaults to running tests in both modes (equivalent to
 * {@code SINGLE_AND_MULTI_PROCESS}).
 *
 * <p>This can be applied to either an individual method or a class. If applied to a class, this
 * applies to each method in the class. In the case where a method and its class have conflicting
 * {@code OnlyRunIn} annotations, the method annotation takes precedence.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface OnlyRunIn {
    public enum ProcessMode {
        SINGLE_PROCESS,
        MULTI_PROCESS,
        SINGLE_AND_MULTI_PROCESS,
        EITHER_PROCESS,
    }

    public ProcessMode value();
}
