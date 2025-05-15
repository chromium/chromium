// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ai;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public abstract class SystemAiProviderFactory {

    public abstract SystemAiProvider createSystemAiProvider();
}
