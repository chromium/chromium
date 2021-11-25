// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.content_public.browser.WebContents;

/**
 * Factory for creating dependencies when running in Chrome.
 */
public class AssistantDependenciesFactoryChrome implements AssistantDependenciesFactory {
    @Override
    public AssistantDependencies createDependencies(WebContents webContents) {
        return new AssistantDependenciesChrome(webContents);
    }

    @Override
    public AssistantStaticDependencies createStaticDependencies() {
        return new AssistantStaticDependenciesChrome() {};
    }
}
