// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * The annotated class should never be horizontally or vertically merged.
 *
 * The annotated classes are guaranteed not to be horizontally or vertically
 * merged by Proguard. Other optimizations may still apply.
 */
@Target({ElementType.TYPE})
@Retention(RetentionPolicy.CLASS)
public @interface DoNotClassMerge {}
