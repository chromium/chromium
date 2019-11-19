// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import android.util.Pair;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import java.util.ArrayList;
import java.util.List;

/**
 * A TestRule for overriding Dagger Module factories.
 */
public class ModuleOverridesRule extends TestWatcher {
    private final List<Pair<Class<?>, ?>> mOverrides = new ArrayList<>();

    /** Override the Module factory of specified type. */
    public <T> ModuleOverridesRule setOverride(Class<T> factoryClass, T override) {
        mOverrides.add(new Pair<>(factoryClass, override));
        return this;
    }

    @Override
    protected void starting(Description description) {
        for (Pair<Class<?>, ?> override : mOverrides) {
            ModuleFactoryOverrides.setOverride(override.first, override.second);
        }
    }

    @Override
    protected void finished(Description description) {
        ModuleFactoryOverrides.clearOverrides();
    }
}
