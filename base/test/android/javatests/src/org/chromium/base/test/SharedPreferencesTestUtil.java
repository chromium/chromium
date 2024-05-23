// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.InMemorySharedPreferences;

import java.io.File;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

class SharedPreferencesTestUtil {
    // These are SharedPreferences that we do not fail tests for using real (instead of in-memory)
    // implementations. This is generally because it is impractical to use a test-only Context
    // for them (e.g. multi-process, or third-party code).
    private static final List<String> SHARED_PREFS_ALLOWLIST =
            List.of(
                    "AwComponentUpdateServicePreferences",
                    "AwOriginVisitLoggerPrefs",
                    "ComponentsProviderServicePreferences",
                    "WebViewChromiumPrefs",
                    "org.chromium.android_webview.devui.MainActivity",
                    "org.chromium.webengine.test.instrumentation_test_apk_preferences");

    static void deleteOnDiskSharedPreferences(Application app) {
        for (String name : findSharedPreferences()) {
            // Using Application ensure we are not using InMemorySharedPreferencesContext.
            app.deleteSharedPreferences(name);
        }
    }

    static void assertNoOnDiskSharedPreferences() {
        Collection<String> unwantedPrefs = findSharedPreferences();
        unwantedPrefs.removeAll(SHARED_PREFS_ALLOWLIST);
        if (unwantedPrefs.isEmpty()) {
            return;
        }
        String errorMsg =
                """
                Found unexpected shared preferences file(s) after test ran.

                All code should use ContextUtils.getApplicationContext() when accessing
                SharedPreferences so that test are hooked to use InMemorySharedPreferences. It may
                also be necessary to override getSharedPreferences() on Context subclasses
                (e.g. ChromeBaseAppCompatActivity does this to make Preferences screens work).

                If it is not practical to have tests use an InMemorySharedPreferences in you case,
                then you can add the shared preferences to SHARED_PREFS_ALLOWLIST.
                """;

        SharedPreferences testPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences("test", Context.MODE_PRIVATE);
        if (!(testPrefs instanceof InMemorySharedPreferences)) {
            errorMsg +=
                    String.format(
                            "\nContextUtils.getApplicationContext() was set to type \"%s\", which"
                                    + " does not delegate to InMemorySharedPreferencesContext (this"
                                    + " is likely the issues).\n",
                            ContextUtils.getApplicationContext().getClass().getName());
        }

        errorMsg += "Shared Preferences:\n * " + TextUtils.join("\n * ", unwantedPrefs);
        throw new AssertionError(errorMsg);
    }

    private static Collection<String> findSharedPreferences() {
        File dataDir = ContextCompat.getDataDir(BaseJUnit4ClassRunner.getApplication());
        File prefsDir = new File(dataDir, "shared_prefs");
        File[] files = prefsDir.listFiles();
        Set<String> ret = new HashSet<>();
        if (files != null) {
            for (File f : files) {
                if (f.getName().endsWith(".xml")) {
                    ret.add(f.getName().substring(0, f.getName().length() - 4));
                } else if (f.getName().endsWith(".xml.bak")) {
                    ret.add(f.getName().substring(0, f.getName().length() - 8));
                }
            }
        }
        return ret;
    }
}
