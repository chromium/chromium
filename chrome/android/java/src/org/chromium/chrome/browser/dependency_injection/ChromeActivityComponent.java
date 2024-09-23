// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import dagger.Subcomponent;

/** Activity-scoped component associated with {@link org.chromium.chrome.browser.ChromeActivity}. */
// TODO(crbug.com/41453884): Remove this and fix dependencies.
@Subcomponent(modules = {ChromeActivityCommonsModule.class})
@ActivityScope
public interface ChromeActivityComponent {
    ChromeAppComponent getParent();
}
