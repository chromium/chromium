// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.vr.rules.ArPlaybackFile;
import org.chromium.chrome.browser.vr.rules.ArTestRule;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityArTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityArTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityArTestRule;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.Callable;

/** Utility class for interacting with AR-specific rules. */
public class ArTestRuleUtils extends XrTestRuleUtils {
    private static final String DEFAULT_PLAYBACK_FILEPATH =
            "chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4";
    // This must be kept in sync with the ArCore session_settings entry in
    // //chrome/android/javatests/AndroidManifest_monochrome.xml.
    private static final String PLAYBACK_FILE_LOCATION =
            UrlUtils.getIsolatedTestFilePath("chrome/test/data/xr/dataset.mp4");

    private static final int BLOCK_SIZE = 2048;

    /**
     * Helper method to apply an ArTestRule/ChromeActivityTestRule combination. The only difference
     * between various classes that implement ArTestRule is how they start their activity, so the
     * common boilerplate code can be kept here so each ArTestRule only has to provide a way to
     * launch Chrome.
     *
     * @param base The Statement passed to the calling ChromeActivityRule's apply() method.
     * @param desc The Description passed to the calling ChromeActivityTestRule's apply() method.
     * @param rule The calling ArTestRule.
     * @param launcher A ChromeLaunchMethod whose launch() contains the code snippet to start Chrome
     *     in the calling ChromeActivityTestRule's activity type.
     */
    public static void evaluateArTestRuleImpl(
            final Statement base,
            final Description desc,
            final ArTestRule rule,
            final ChromeLaunchMethod launcher)
            throws Throwable {
        // See if the test is annotated with @ArPlaybackFile
        String playbackFilepath = DEFAULT_PLAYBACK_FILEPATH;
        ArPlaybackFile annotation = desc.getAnnotation(ArPlaybackFile.class);
        if (annotation != null) playbackFilepath = annotation.value();
        playbackFilepath = UrlUtils.getIsolatedTestFilePath(playbackFilepath);

        // Check that the playback file actually exists, and if so, copy it to the location that's
        // hard-coded into the manifest so that ArCore can find and use it.
        File playbackFile = new File(playbackFilepath);
        Assert.assertTrue(
                "Given AR playback file " + playbackFilepath + " does not exist.",
                playbackFile.exists());
        Assert.assertFalse(
                "Given AR playback file " + playbackFilepath + " is a directory.",
                playbackFile.isDirectory());
        copyDatasetIfNecessary(playbackFile, new File(PLAYBACK_FILE_LOCATION));

        launcher.launch();
        base.evaluate();
    }

    /**
     * Creates the list of ArTestRules that are currently supported for use in test
     * parameterization.
     *
     * @return An ArrayList of ParameterSets, with each ParameterSet containing a callable to create
     *     an ArTestRule for a supported ChromeActivity.
     */
    public static ArrayList<ParameterSet> generateDefaultTestRuleParameters() {
        ArrayList<ParameterSet> parameters = new ArrayList<ParameterSet>();
        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<ChromeTabbedActivityArTestRule>() {
                                    @Override
                                    public ChromeTabbedActivityArTestRule call() {
                                        return new ChromeTabbedActivityArTestRule();
                                    }
                                })
                        .name("ChromeTabbedActivity"));
        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<CustomTabActivityArTestRule>() {
                                    @Override
                                    public CustomTabActivityArTestRule call() {
                                        return new CustomTabActivityArTestRule();
                                    }
                                })
                        .name("CustomTabActivity"));
        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<WebappActivityArTestRule>() {
                                    @Override
                                    public WebappActivityArTestRule call() {
                                        return new WebappActivityArTestRule();
                                    }
                                })
                        .name("WebappActivity"));
        return parameters;
    }

    /**
     * Copies |src| to |dst| if the contents of the two are not already identical.
     *
     * @param src The File to copy data from.
     * @param dst The File to copy data to.
     */
    private static void copyDatasetIfNecessary(File src, File dst)
            throws NoSuchAlgorithmException, FileNotFoundException, IOException {
        if (!dst.exists()) {
            copyDataset(src, dst);
            return;
        }
        if (!Arrays.equals(getFileDigest(src), getFileDigest(dst))) {
            copyDataset(src, dst);
        }
    }

    /**
     * Computes the SHA-1 digest of |src|.
     *
     * @param src The File to compute the digest of.
     * @return A byte array containing the SHA-1 digest of |src|.
     */
    private static byte[] getFileDigest(File src)
            throws NoSuchAlgorithmException, FileNotFoundException, IOException {
        MessageDigest md = MessageDigest.getInstance("SHA-1");
        try (FileInputStream in = new FileInputStream(src)) {
            byte[] buf = new byte[BLOCK_SIZE];
            int len;
            while ((len = in.read(buf)) > 0) {
                md.update(buf, 0, len);
            }
        }
        return md.digest();
    }

    /**
     * Copies |src| to |dst|.
     *
     * @param src The file to copy data from.
     * @param dst The file to copy data to.
     */
    private static void copyDataset(File src, File dst) throws FileNotFoundException, IOException {
        // Files.copy() would be much simpler than this, but is only available on API version 26+.
        try (FileInputStream in = new FileInputStream(src)) {
            try (FileOutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[BLOCK_SIZE];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
            }
        }
    }
}
