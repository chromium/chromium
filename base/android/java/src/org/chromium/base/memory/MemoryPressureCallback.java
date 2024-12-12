// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import org.chromium.base.MemoryPressureLevel;
import org.chromium.build.annotations.NullMarked;

/** Memory pressure callback interface. */
@NullMarked
@FunctionalInterface
public interface MemoryPressureCallback {
    public void onPressure(@MemoryPressureLevel int pressure);
}
