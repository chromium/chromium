// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

/**
 * Based on Java 8's java.util.function.BooleanSupplier. Same as Supplier<T>, but with a primitive
 * little b boolean type. TODO(https://crbug.com/1034012): Remove once min Android API level reaches
 * 24.
 */
public interface BooleanSupplier {
    /** Returns a value. */
    boolean getAsBoolean();
}
