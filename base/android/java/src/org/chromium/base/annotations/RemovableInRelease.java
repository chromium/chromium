// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/**
 * Methods with this annotation will always be removed in release builds.
 * If they cannot be safely removed, then the build will break.
 *
 * "Safely removed" refers to how return values are used. The current
 * ProGuard rules are configured such that:
 *  - methods that return an object will always return null.
 *  - methods that return boolean will always return false.
 *  - methods that return other primitives are removed so long as their return
 *    values are not used.
 */
@Target({ElementType.METHOD})
public @interface RemovableInRelease {}
