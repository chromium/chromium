// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation used to mark field that may contain Strings referring to fully qualified class names
 * and methods whose arguments may be fully qualified class names. These classes may then be
 * obfuscated by R8. A couple caveats when using this:
 * - This only obfuscates the string, it does not actually check that the class exists.
 * - If a field has this annotation, it must be non-final, otherwise javac will inline the constant
 *   and R8 won't obfuscate it.
 * - Any field/method must be assigned/called with a String literal or a variable R8 can easily
 *   trace to a String literal.
 *
 * <p>Usage example:<br>
 *   {@code
 *   @IdentifierNameString
 *   public static final String LOGGING_TAG = "com.google.android.apps.foo.FooActivity";
 *
 *   // In this example, both className and message are treated as identifier name strings, but will
 *   // only be obfuscated if the string points to a real class.
 *   @IdentifierNameString
 *   public void doSomeLogging(String className, String message) { ... }
 *   }
 */
@Target({ElementType.FIELD, ElementType.METHOD})
@Retention(RetentionPolicy.CLASS)
public @interface IdentifierNameString {}
