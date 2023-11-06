// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DirectoryOption;

import java.util.ArrayList;

/** Unit test for {@link DownloadDialogUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadDialogUtilsUnitTest {
    private final DirectoryOption mInternalSmallOption =
            new DirectoryOption(
                    "/internal", 301, 1000, DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
    private final DirectoryOption mInternalLargeOption =
            new DirectoryOption(
                    "/internal", 600, 1000, DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
    private final DirectoryOption mExternalSmallOption =
            new DirectoryOption(
                    "/sd_card",
                    100,
                    2000,
                    DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL);
    private final DirectoryOption mExternalLargeOption =
            new DirectoryOption(
                    "/sd_card",
                    1000,
                    2000,
                    DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL);
    private final String mDefaultLocation = "/internal";

    @Test
    public void testShouldSuggestDownloadLocation_DefaultEnoughSpace() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        dirs.add(mInternalLargeOption);
        dirs.add(mExternalLargeOption);
        assertFalse(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 300));
    }

    @Test
    public void testShouldSuggestDownloadLocation_UnknownBytes() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        dirs.add(mInternalLargeOption);
        dirs.add(mExternalLargeOption);
        assertFalse(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 0));
    }

    @Test
    public void testShouldSuggestDownloadLocation_SuggestExternal() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        dirs.add(mInternalSmallOption);
        dirs.add(mExternalLargeOption);
        assertTrue(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 300));
    }

    @Test
    public void testShouldSuggestDownloadLocation_BothNotEnoughSPace() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        dirs.add(mInternalSmallOption);
        dirs.add(mExternalSmallOption);
        assertFalse(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 300));
    }

    @Test
    public void testShouldSuggestDownloadLocation_NoAvailableStorage() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        assertFalse(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 300));
    }

    @Test
    public void testShouldSuggestDownloadLocation_NoExternalStorage() {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();
        dirs.add(mInternalLargeOption);
        assertFalse(DownloadDialogUtils.shouldSuggestDownloadLocation(dirs, mDefaultLocation, 300));
    }
}
