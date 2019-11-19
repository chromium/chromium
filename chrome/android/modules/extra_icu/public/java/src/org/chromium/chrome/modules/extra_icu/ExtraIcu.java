// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.extra_icu;

import org.chromium.components.module_installer.builder.ModuleInterface;

/** Interface into the extra ICU module. Only used to check whether module is installed. */
@ModuleInterface(module = "extra_icu", impl = "org.chromium.chrome.modules.extra_icu.ExtraIcuImpl")
public interface ExtraIcu {}
