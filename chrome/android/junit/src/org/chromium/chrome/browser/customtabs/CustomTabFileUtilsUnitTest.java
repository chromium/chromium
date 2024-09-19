// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import org.hamcrest.Matchers;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Test for {@link CustomTabFileUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabFileUtilsUnitTest {
    private static List<File> generateMaximumStateFiles(long currentTime) {
        List<File> validFiles = new ArrayList<>();
        for (int i = 0; i < CustomTabFileUtils.MAXIMUM_STATE_FILES; i++) {
            validFiles.add(buildTestFile("testfile" + i, currentTime));
        }
        return validFiles;
    }

    private static File buildTestFile(String filename, final long lastModifiedTime) {
        return new File(filename) {
            @Override
            public long lastModified() {
                return lastModifiedTime;
            }
        };
    }

    @Test
    public void testDeletableMetadataSelection_NoFiles() {
        List<File> deletableFiles =
                CustomTabFileUtils.getFilesForDeletion(
                        System.currentTimeMillis(), new ArrayList<File>());
        assertThat(deletableFiles, Matchers.emptyIterableOf(File.class));
    }

    @Test
    public void testDeletableMetadataSelection_MaximumValidFiles() {
        long currentTime = System.currentTimeMillis();

        // Test the maximum allowed number of state files where they are all valid in terms of age.
        List<File> filesToTest = new ArrayList<>();
        filesToTest.addAll(generateMaximumStateFiles(currentTime));
        List<File> deletableFiles =
                CustomTabFileUtils.getFilesForDeletion(currentTime, filesToTest);
        assertThat(deletableFiles, Matchers.emptyIterableOf(File.class));
    }

    @Test
    public void testDeletableMetadataSelection_ExceedsMaximumValidFiles() {
        long currentTime = System.currentTimeMillis();

        // Test where we exceed the maximum number of allowed state files and ensure it chooses the
        // older file to delete.
        List<File> filesToTest = new ArrayList<>();
        filesToTest.addAll(generateMaximumStateFiles(currentTime));
        File slightlyOlderFile = buildTestFile("slightlyolderfile", currentTime - 1L);
        // Insert it into the middle just to ensure it is not picking the last file.
        filesToTest.add(filesToTest.size() / 2, slightlyOlderFile);
        List<File> deletableFiles =
                CustomTabFileUtils.getFilesForDeletion(currentTime, filesToTest);
        assertThat(deletableFiles, Matchers.containsInAnyOrder(slightlyOlderFile));
    }

    @Test
    public void testDeletableMetadataSelection_ExceedExpiryThreshold() {
        long currentTime = System.currentTimeMillis();

        // Ensure that files that exceed the allowed time threshold are removed regardless of the
        // number of possible files.
        List<File> filesToTest = new ArrayList<>();
        File expiredFile =
                buildTestFile(
                        "expired_file", currentTime - CustomTabFileUtils.STATE_EXPIRY_THRESHOLD);
        filesToTest.add(expiredFile);
        List<File> deletableFiles =
                CustomTabFileUtils.getFilesForDeletion(currentTime, filesToTest);
        assertThat(deletableFiles, Matchers.containsInAnyOrder(expiredFile));
    }
}
