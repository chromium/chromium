// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Configuration fields for WebContent.
 *
 * <p>This annotation lists the possible configuration keys that can be passed to a WebContent
 * builder or configurer across the support library boundary. These keys are meant to be used in
 * conjunction with values through a key-value mapping mechanism to define properties for a
 * WebContent instance.
 */
@NullMarked
@Target(ElementType.TYPE_USE)
@IntDef({})
@Retention(RetentionPolicy.SOURCE)
public @interface WebContentConfig {}
