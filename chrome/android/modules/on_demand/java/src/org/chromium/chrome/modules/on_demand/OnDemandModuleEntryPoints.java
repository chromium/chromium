// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.on_demand;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * Interface for the implementation that lives inside the apk split (the interface itself lives in
 * the base split)
 */
@ModuleInterface(
        module = "on_demand",
        impl = "org.chromium.chrome.modules.on_demand.OnDemandModuleEntryPointsImpl")
@NullMarked
public interface OnDemandModuleEntryPoints {
    /** Used by internal Chrome builds. */
    @Nullable Object getInternalEntryPoints();
}
