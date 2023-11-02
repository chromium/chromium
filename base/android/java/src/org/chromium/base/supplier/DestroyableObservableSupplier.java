// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.lifetime.Destroyable;

/**
 * An {@link ObservableSupplier} that may be destroyed by anyone with a reference to the object.
 * This is useful if the class that constructs the object implementing this interface is not
 * responsible for its cleanup. For example, this may be useful when constructing an object
 * using the factory pattern.
 *
 * @param <E> The type of the wrapped object.
 */
public interface DestroyableObservableSupplier<E> extends ObservableSupplier<E>, Destroyable {}
