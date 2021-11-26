// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.ui.util.AccessibilityUtil;

/**
 * Generic static dependencies interface. The concrete implementation will depend on the browser
 * framework, i.e., WebLayer vs. Chrome.
 */
public interface AssistantStaticDependencies {
    AccessibilityUtil getAccessibilityUtil();
}
