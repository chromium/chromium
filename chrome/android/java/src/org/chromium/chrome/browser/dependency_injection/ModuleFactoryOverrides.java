// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.Map;

/**
 * This class is for overriding factories of Modules in tests. See also ModuleOverridesRule.
 *
 * The chosen approach for substituting dagger bindings in integrational tests is to override
 * Modules, without touching Components. Substituting the entire structure of Components in each
 * suite of tests is unreasonably cumbersome.
 *
 * Overriding the Modules themselves would not be a good enough solution in general,
 * because modules have parameters known only at creation time (such as parameters extracted from
 * Intent extras in an activity). Plus, in some tests there might be multiple instances of Modules
 * of the same type. Thus, instead of Modules we need to override factories of Modules.
 *
 * The factory overrides are set by tests as the first step of initialization. The production code
 * then calls {@link #getOverrideFor} to obtain the overridden factory, if any.
 *
 * Note that for unit tests you don't need to use this: create an instance of tested class directly
 * with "new", and pass the mocks.
 */
public class ModuleFactoryOverrides {
    @Nullable private static Map<Class<?>, Object> sOverrides;

    /** Override the Module factory of specified type. */
    static void setOverride(Class<?> factoryClass, Object override) {
        if (sOverrides == null) {
            sOverrides = new HashMap<>();
        }
        sOverrides.put(factoryClass, override);
    }

    /** Clear all overrides */
    static void clearOverrides() {
        sOverrides = null;
    }

    /** Returns an overridden factory for the given factory class, if present. */
    @Nullable
    @SuppressWarnings("unchecked") // Unsafe cast occurs only in test environment
    public static <T> T getOverrideFor(Class<T> factoryClass) {
        if (sOverrides == null) {
            return null;
        }
        Object overriddenFactory = sOverrides.get(factoryClass);
        if (overriddenFactory != null) {
            return (T) overriddenFactory;
        }
        return null;
    }

    private ModuleFactoryOverrides() {}
}
