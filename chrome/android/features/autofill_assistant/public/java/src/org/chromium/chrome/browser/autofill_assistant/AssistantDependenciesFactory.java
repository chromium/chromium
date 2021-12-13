// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

/**
 * Factory for creating dependencies. Implementations might differ depending on where Autofill
 * Assistant is running (e.g. WebLayer, Chrome).
 */
public interface AssistantDependenciesFactory {
    /**
     * Create the Activity specific dependencies.
     * */
    AssistantDependencies createDependencies(Activity activity);

    /**
     * Create the static dependencies that are independent of Activity.
     * */
    AssistantStaticDependencies createStaticDependencies();
}
