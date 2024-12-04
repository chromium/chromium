// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Based on jspecify's @Nullable and checked by NullAway.
 *
 * <p>Not directly using jspecify's annotations so that Cronet does not need the extra dep.
 *
 * <p>See: https://github.com/uber/NullAway/wiki/Supported-Annotations
 */
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.CLASS)
public @interface Nullable {}
