// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This can be used to set the locale to a specified value before initializing feature list. The
 * passed locale must be a IETF BCP 47 language tag string. The passed locale will be passed to
 * Locale.setDefault(Locale.forLanguageTag()).
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface TestLocale {
    String value();
}
