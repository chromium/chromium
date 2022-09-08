// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/**
 * See b/147584922. Proguard and Mockito don't play nicely together, and proguard rules make it
 * impossible to keep the base class/interface for a mocked class without providing additional
 * explicit information, like this annotation. This annotation should only need to be used on a
 * class/interface that is extended/implemented by another class/interface that is then mocked.
 */
@Target(ElementType.TYPE)
public @interface MockedInTests {}
