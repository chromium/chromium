// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Classes with native methods (contain @NativeMethods interfaces) that are used within renderer
 * processes must be annotated with with @MainDex in order for their native methods work.
 *
 * Applies only for Chrome/ChromeModern (not needed for Monochrome+).
 *
 * For Cronet builds, which use a default_min_sdk_version of less than 21, this annotation also
 * causes classes to appear in the main dex file (for "Legacy multidex").
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface MainDex {}
