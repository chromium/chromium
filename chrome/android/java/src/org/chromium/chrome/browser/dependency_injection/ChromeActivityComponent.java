// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsCoordinator;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsModule;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with {@link org.chromium.chrome.browser.ChromeActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, ContextualSuggestionsModule.class})
@ActivityScope
public interface ChromeActivityComponent {
    // Temporary getters for DI migration process.
    ContextualSuggestionsCoordinator resolveContextualSuggestionsCoordinator();
}
