// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/**
 * Prevents optimization. R8 supports Mockito now, so this should be rarely be needed.
 */
@Target(ElementType.TYPE)
public @interface MockedInTests {}
