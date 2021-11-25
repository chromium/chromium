// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Provides default implemtations of {@link AssistantStaticDependencies} for Chrome.
 */
public interface AssistantStaticDependenciesChrome extends AssistantStaticDependencies {
    @Override
    default AccessibilityUtil getAccessibilityUtil() {
        return ChromeAccessibilityUtil.get();
    }
}
