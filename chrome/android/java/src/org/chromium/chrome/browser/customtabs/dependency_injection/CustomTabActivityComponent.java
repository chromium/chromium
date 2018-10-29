// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityUi;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsModule;
import org.chromium.chrome.browser.customtabs.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, ContextualSuggestionsModule.class,
                      CustomTabActivityModule.class})
@ActivityScope
public interface CustomTabActivityComponent extends ChromeActivityComponent {
    TrustedWebActivityUi resolveTrustedWebActivityUi();
    CustomTabBrowserControlsVisibilityDelegate resolveControlsVisibilityDelegate();
}
