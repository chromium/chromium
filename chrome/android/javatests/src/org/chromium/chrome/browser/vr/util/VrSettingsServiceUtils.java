// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.content.ComponentName;
import android.content.Intent;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.runner.Description;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.vr.rules.VrSettingsFile;
import org.chromium.chrome.browser.vr.rules.VrTestRule;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Scanner;

/**
 * Utility class for interacting with the VrCore developer settings service, which allows tests to
 * change VrCore's configuration without applying a shared preference file.
 *
 * Requires that the "EnableDeveloperService" setting is enabled.
 */
public class VrSettingsServiceUtils {
    // Notable properties: DON flow enabled, DDView 2016 paired, VrCore-side emulated controller
    // disabled, VrCore head tracking service disabled.
    public static final String FILE_DDVIEW_DONENABLED = "ddview_donenabled.txt";

    private static final ComponentName SETTINGS_SERVICE_COMPONENT = new ComponentName(
            "com.google.vr.vrcore", "com.google.vr.vrcore.developer.VrDeveloperService");
    private static final String SETTINGS_FILE_DIR =
            "chrome/test/data/xr/e2e_test_files/vr_settings_files/";
    private static final String EXTRA_ACTION = "action";
    private static final String EXTRA_ACTION_VALUE = "UPDATE_SETTINGS";
    private static final String EXTRA_VR_SETTINGS_PATH = "vr_settings_path";

    // Needs to be long because this can occasionally take multiple seconds to apply.
    private static final int SETTINGS_APPLICATION_DELAY_MS = 3000;

    /**
     * Applies the settings specified in the provided file to VrCore.
     *
     * @param filename The name of the file in SETTINGS_FILE_DIR to apply.
     * @param rule The VrTestRule for the current test.
     */
    public static void applySettingsFile(String filename, VrTestRule rule)
            throws FileNotFoundException {
        // We only want to be applying settings this way when explicitly running tests that use this
        // functionality since it persists between tests. The shared preference files applied for
        // tests that are not expected to use this should cause application of settings this way
        // to be a NOOP, but getting to this method without explicitly wanting this functionality
        // is indicative of a problem, so fail.
        Assert.assertTrue("Attempted to use the VR settings service without explicitly allowing it",
                CommandLine.getInstance().hasSwitch("vr-settings-service-enabled"));

        // Start the service and have it update VrCore settings using the provided file.
        Intent settingsIntent = new Intent();
        settingsIntent.putExtra(EXTRA_ACTION, EXTRA_ACTION_VALUE);
        settingsIntent.putExtra(EXTRA_VR_SETTINGS_PATH,
                UrlUtils.getIsolatedTestFilePath(SETTINGS_FILE_DIR) + filename);
        settingsIntent.setComponent(SETTINGS_SERVICE_COMPONENT);
        Assert.assertTrue("Failed to start VR setttings service",
                InstrumentationRegistry.getContext().startService(settingsIntent) != null);

        // We need to tell the rule whether the newly applied settings enable the DON flow.
        rule.setDonEnabled(fileEnablesDon(filename));
        // The settings usually apply nearly instantly, but can occasionally take multiple seconds.
        // Since this is done through a service, we don't have any guarantees about that or a way to
        // be notified when they're applied. So, sleep for a while. This is a long sleep, but few
        // tests use this functionality, so overall runtime impact is pretty low.
        SystemClock.sleep(SETTINGS_APPLICATION_DELAY_MS);
    }

    /**
     * Checks if a test is annotated with a VrSettingsFile annotation, and if so, applies the
     * given settings file. Only meant to be used by a Rule's apply() method.
     *
     * @param desc The Description object for the Rule currently being applied.
     * @param rule The VrTestRule currently being applied.
     */
    public static void checkForAndApplyVrSettingsFileAnnotation(Description desc, VrTestRule rule)
            throws FileNotFoundException {
        // Check if the test has a VrSettingsFile annotation
        VrSettingsFile annotation = desc.getAnnotation(VrSettingsFile.class);
        if (annotation == null) return;
        applySettingsFile(annotation.value(), rule);
    }

    private static boolean fileEnablesDon(String filename) throws FileNotFoundException {
        File infile = new File(UrlUtils.getIsolatedTestFilePath(SETTINGS_FILE_DIR) + filename);
        Scanner scanner = new Scanner(infile);
        for (String curLine = scanner.nextLine(); scanner.hasNextLine();
                curLine = scanner.nextLine()) {
            if (!curLine.contains("VrSkipDon")) continue;
            if (curLine.contains("true"))
                return false;
            else if (curLine.contains("false"))
                return true;
            Assert.fail(
                    "Given file " + filename + " malformed - VrSkipDon present, but not a bool");
        }
        Assert.fail("Given file " + filename
                + " does not contain VrSkipDon. Please explicitly enable or disable the DON flow");
        return false;
    }
}