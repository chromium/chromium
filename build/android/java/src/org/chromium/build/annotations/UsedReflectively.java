// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Tells Error Prone that annotated types should not be considered unused.
 *
 * <p>Package-private since other users should use com.google.errorprone.annotations.Keep, which is
 * not used here in order to avoid a dependency from //build.
 */
@Target({ElementType.ANNOTATION_TYPE})
@Retention(RetentionPolicy.CLASS)
@interface UsedReflectively {}
