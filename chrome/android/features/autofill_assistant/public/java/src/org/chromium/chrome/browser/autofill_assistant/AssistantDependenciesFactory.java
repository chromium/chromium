// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.content_public.browser.WebContents;

/**
 * Factory for creating dependencies. Implementations might differ depending on where Autofill
 * Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantDependenciesFactory {
    /**
     * Create the WebContents specific dependencies.
     * */
    AssistantDependencies createDependencies(WebContents webContents);

    /**
     * Create the static dependencies that are independent of WebContents.
     * */
    AssistantStaticDependencies createStaticDependencies();
}
