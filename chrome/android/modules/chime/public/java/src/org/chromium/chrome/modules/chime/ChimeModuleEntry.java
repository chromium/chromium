// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.chime;

import org.chromium.components.module_installer.builder.ModuleInterface;

/**
 * Use this module to register to Chime notification platform.
 */
@ModuleInterface(module = "chime", impl = "org.chromium.chrome.modules.chime.ChimeModuleEntryImpl")
public interface ChimeModuleEntry {
    /**
     * Registers the Chime notification SDK to the server.
     */
    void register();
}
