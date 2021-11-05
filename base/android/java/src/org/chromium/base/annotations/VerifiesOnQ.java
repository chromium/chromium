// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * The annotated method or class verifies on Q, but not below.
 *
 * The annotated method (or methods on the annotated class) are guaranteed to not be inlined by R8
 * on builds targeted below Q. This prevents class verification errors (which results in a very slow
 * retry-verification-at-runtime) from spreading into other classes on these lower versions.
 */
@Target({ElementType.CONSTRUCTOR, ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.CLASS)
public @interface VerifiesOnQ {}
