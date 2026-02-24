// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * An interface for a two-argument RepeatingCallback that must be manually destroyed.
 *
 * @param <T1> The type of the first result.
 * @param <T2> The type of the second result.
 */
@NullMarked
public interface JniRepeatingCallback2<T1 extends @Nullable Object, T2 extends @Nullable Object>
        extends Callback2<T1, T2>, Destroyable {}
