// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.NonNull;

import org.junit.runners.model.InitializationError;
import org.robolectric.TestLifecycle;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.reflect.Method;

/**
 * A Robolectric Test Runner that initializes and resets Chrome globals between tests. The //base
 * state is managed by {@link BaseRobolectricTestRunner}.
 */
public class ChromeRobolectricTestRunner extends BaseRobolectricTestRunner {
    public ChromeRobolectricTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    public static class ChromeTestLifecycle extends BaseRobolectricTestRunner.BaseTestLifecycle {
        @Override
        public void beforeTest(Method method) {
            super.beforeTest(method);
            ColdStartTracker.initialize();
        }

        @Override
        public void afterTest(Method method) {
            ColdStartTracker.resetInstanceForTesting();
            // DisplayAndroidManager will reuse the Display between tests. This can cause
            // AsyncInitializationActivity#applyOverrides to set incorrect smallestWidth.
            DisplayAndroidManager.resetInstanceForTesting();
            TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
            AdaptiveToolbarStatePredictor.setToolbarStateForTesting(null);
            super.afterTest(method);
        }
    }

    @NonNull
    @Override
    protected Class<? extends TestLifecycle> getTestLifecycleClass() {
        return ChromeTestLifecycle.class;
    }
}
