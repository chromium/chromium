// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Causes a method to treated as a constructor for the sake of null checking.
 *
 * <p>See: https://github.com/uber/NullAway/wiki/Supported-Annotations
 *
 * <p>Not directly using NullAway's annotations so that Cronet does not need the extra dep.
 */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.CLASS)
public @interface Initializer {}
