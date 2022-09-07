// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation that runs a test method on the UI thread.
 *
 * This is used in place of android.support.test.annotation.UiThreadTest, as ActivityTestRule will
 * run on the UI thread if that annotation is present, possibly causing other Rules to unexpectedly
 * run on the UI thread as well.
 *
 * UiThreadTestRule should be avoided altogether, as it causes @Before and @After to run on the UI
 * thread, which most test writers do not expect.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface UiThreadTest {}
