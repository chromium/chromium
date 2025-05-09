// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

/*
 * A wrapper that owns a native side base::OnceCallback.
 *
 * You must call JniOnceCallback#destroy() if you never end up calling onResult
 * so as to not leak the native callback.
 *
 * This class has no additional thread safety measures compared to
 * base::RepeatingCallback.
 */
@NullMarked
public interface JniOnceRunnable extends Runnable, Destroyable {}
