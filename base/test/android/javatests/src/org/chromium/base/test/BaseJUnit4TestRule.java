// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;

import java.io.File;
import java.util.ArrayList;

/**
 * Holds setUp / tearDown logic common to all instrumentation tests.
 */
class BaseJUnit4TestRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Don't tests if there are prior on-disk shared prefs lying around.
                checkOrDeleteOnDiskSharedPreferences(false);

                InMemorySharedPreferencesContext context =
                        BaseChromiumAndroidJUnitRunner.sInMemorySharedPreferencesContext;
                if (context == null) {
                    throw new IllegalStateException("BaseJUnit4TestRule requires that you use "
                            + "BaseChromiumAndroidJUnitRunner (or a subclass)");
                }
                // Reset Application context in case any tests have replaced it.
                ContextUtils.initApplicationContextForTests(context);
                // Ensure all tests start with empty (InMemory)SharedPreferences.
                context.clearSharedPreferences();
                // Delete any files that leak state between tests.
                clearDataDirectory(context);

                base.evaluate();

                // Do not use try/finally so that preferences asserts do not mask prior exceptions.
                checkOrDeleteOnDiskSharedPreferences(true);
            }
        };
    }

    private void checkOrDeleteOnDiskSharedPreferences(boolean check) {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File prefsDir = new File(dataDir, "shared_prefs");
        File[] files = prefsDir.listFiles();
        if (files == null) {
            return;
        }
        ArrayList<File> badFiles = new ArrayList<>();
        for (File f : files) {
            // Multidex support library prefs need to stay or else multidex extraction will occur
            // needlessly.
            // WebView prefs need to stay because webview tests have no (good) way of hooking
            // SharedPreferences for instantiated WebViews.
            if (!f.getName().endsWith("multidex.version.xml")
                    && !f.getName().equals("WebViewChromiumPrefs.xml")) {
                if (check) {
                    badFiles.add(f);
                } else {
                    f.delete();
                }
            }
        }
        if (!badFiles.isEmpty()) {
            String errorMsg = "Found unexpected shared preferences file(s) after test ran.\n"
                    + "All code should use ContextUtils.getApplicationContext() when accessing "
                    + "SharedPreferences so that tests are hooked to use InMemorySharedPreferences."
                    + " This could also mean needing to override getSharedPreferences() on custom "
                    + "Context subclasses (e.g. ChromeBaseAppCompatActivity does this to make "
                    + "Preferences screens work).\n\n";

            SharedPreferences testPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                    "test", Context.MODE_PRIVATE);
            if (!(testPrefs instanceof InMemorySharedPreferences)) {
                errorMsg += String.format(
                        "ContextUtils.getApplicationContext() was set to type \"%s\", which does "
                                + "not delegate to InMemorySharedPreferencesContext (this is "
                                + "likely the issues).\n\n",
                        ContextUtils.getApplicationContext().getClass().getName());
            }

            errorMsg += "Files:\n * " + TextUtils.join("\n * ", badFiles);
            throw new AssertionError(errorMsg);
        }
    }

    private static void clearDataDirectory(Context targetContext) {
        File dataDir = ContextCompat.getDataDir(targetContext);
        File[] files = dataDir.listFiles();
        if (files == null) return;
        for (File file : files) {
            // Symlink to app's native libraries.
            if (file.getName().equals("lib")) {
                continue;
            }
            if (file.getName().equals("incremental-install-files")) {
                continue;
            }
            // E.g. Legacy multidex files.
            if (file.getName().equals("code_cache")) {
                continue;
            }
            // SharedPreferences handled by checkOrDeleteOnDiskSharedPreferences().
            if (file.getName().equals("shared_prefs")) {
                continue;
            }
            if (file.isDirectory()
                    && (file.getName().startsWith("app_") || file.getName().equals("cache"))) {
                // Directories are lazily created by PathUtils only once, and so can be cleared but
                // not removed.
                for (File subFile : file.listFiles()) {
                    if (!FileUtils.recursivelyDeleteFile(subFile)) {
                        throw new RuntimeException(
                                "Could not delete file: " + subFile.getAbsolutePath());
                    }
                }
            } else if (!FileUtils.recursivelyDeleteFile(file)) {
                throw new RuntimeException("Could not delete file: " + file.getAbsolutePath());
            }
        }
    }
}
