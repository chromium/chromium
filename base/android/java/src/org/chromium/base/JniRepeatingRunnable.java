// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

/*
 * A wrapper that owns a native side base::RepeatingCallback.
 *
 * You must call JniRepeatingCallback#destroy() when you are done with it to
 * not leak the native callback.
 *
 * This class has no additional thread safety measures compared to
 * base::RepeatingCallback.
 */
@NullMarked
public interface JniRepeatingRunnable extends Runnable, Destroyable {}
