// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Indicates that a test method is intended to load native libraries.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.METHOD) // can use in method only.
public @interface LoadNative {}
