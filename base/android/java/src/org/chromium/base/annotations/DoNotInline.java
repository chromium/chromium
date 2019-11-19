// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * The annotated method or class should never be inlined.
 *
 * The annotated method (or methods on the annotated class) are guaranteed not to be inlined by
 * Proguard. Other optimizations may still apply. Do not use this annotation to fix class
 * verification errors - use the @VerifiesOnX annotations instead.
 */
@Target({ElementType.CONSTRUCTOR, ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.CLASS)
public @interface DoNotInline {}
