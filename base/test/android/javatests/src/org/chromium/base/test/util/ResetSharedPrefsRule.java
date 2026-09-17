// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.Context;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;

/**
 * A JUnit {@link TestRule} that resets all application {@link android.content.SharedPreferences}
 * after each test method. Should not be used in Robolectric tests, as they already reset shared
 * prefs between test cases. On device unit tests reset between classes, but this is helpful if you
 * still want to reset between test cases.
 */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
public class ResetSharedPrefsRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    resetSharedPreferences();
                }
            }
        };
    }

    private static void resetSharedPreferences() {
        Context context = ContextUtils.getApplicationContext();
        if (context instanceof InMemorySharedPreferencesContext inMemoryContext) {
            inMemoryContext.resetSharedPreferences();
        } else {
            ContextUtils.getAppSharedPreferences().edit().clear().apply();
        }
    }
}
