// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import org.chromium.base.MemoryPressureLevel;

/**
 * Memory pressure callback interface.
 */
@FunctionalInterface
public interface MemoryPressureCallback {
    public void onPressure(@MemoryPressureLevel int pressure);
}
