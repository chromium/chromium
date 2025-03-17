// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Fields that are never null once becoming non-null. Use to avoid warnings when such fields are
 * accessed by lambdas after being assigned.
 *
 * <p>See: https://github.com/uber/NullAway/wiki/Supported-Annotations
 */
@Target(ElementType.FIELD)
@Retention(RetentionPolicy.CLASS)
public @interface MonotonicNonNull {}
