// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.chime;

/**
 * An empty base class for {@link ChimeModuleEntry}. The main purpose of this class is to make both
 * chromium upstream and clank downstream compile at the same time for the DFM module interface
 * {@link ChimeModuleEntry}.
 */
public class ChimeModuleBase implements ChimeModuleEntry {
    @Override
    public void register() {}
}
