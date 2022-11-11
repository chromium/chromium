// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;

import java.util.Optional;

/** Adapter methods to migrate Guava's Optional to Java's. */
public class OptionalUtils {
    public static <T> com.google.common.base.Optional<T> toGuavaOptional(Optional<T> optional) {
        return optional.isPresent() ? com.google.common.base.Optional.of(optional.get())
                                    : com.google.common.base.Optional.absent();
    }

    public static <T> Callback<com.google.common.base.Optional<T>> toGuavaOptionalCallback(
            Callback<Optional<T>> callback) {
        return (com.google.common.base.Optional<T> optional) -> {
            callback.onResult(
                    optional.isPresent() ? Optional.of(optional.get()) : Optional.empty());
        };
    }
}
