// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.chime;

import org.chromium.base.annotations.UsedByReflection;

/**
 * Implementation of Chime DFM module hook. This is the upstream one that does nothing, the actual
 * implementation lives in downstream.
 */
@UsedByReflection("ChimeModule")
public class ChimeModuleEntryImpl implements ChimeModuleEntry {
    @Override
    public void register() {}
}
