// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * See: https://github.com/uber/NullAway/wiki/Supported-Annotations#contracts
 *
 * <p>Not directly using NullAway's annotations so that Cronet does not need the extra dep.
 *
 * <pre>
 * Examples:
 * // The contract is: "If the parameter is null, the method will return false".
 * // NullAway infers nullness from the inverse: Returning "true" means the parameter is non-null.
 * @Contract("null -> false")
 * // Returning false means the second parameter is non-null.
 * @Contract("_, null -> true")
 * </pre>
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.CLASS)
public @interface Contract {
    String value();
}
