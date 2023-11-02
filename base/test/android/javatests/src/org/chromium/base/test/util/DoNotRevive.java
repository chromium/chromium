// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation is for disabled tests that should not be run in Test Reviver. Tests that cause
 * other tests to fail or caue problems to the testing the infrastructure should have this
 * annotation. <p> This should be used in conjunction with @DisabledTest or @DisableIf to prevent a
 * test from running on normal bots.
 */
@Target({ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
public @interface DoNotRevive {
    String reason();
}
