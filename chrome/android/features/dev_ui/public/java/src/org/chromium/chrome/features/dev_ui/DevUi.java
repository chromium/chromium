// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.dev_ui;

import org.chromium.components.module_installer.builder.ModuleInterface;

/** Interface to call into DevUI feature. */
@ModuleInterface(module = "dev_ui", impl = "org.chromium.chrome.features.dev_ui.DevUiImpl")
public interface DevUi {}
