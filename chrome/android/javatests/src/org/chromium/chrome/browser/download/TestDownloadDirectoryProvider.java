// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;

/**
 * Used to provide arbitary number of download directories in tests.
 */
public class TestDownloadDirectoryProvider extends DownloadDirectoryProvider {
    private ArrayList<DirectoryOption> mDirectoryOptions;

    public TestDownloadDirectoryProvider(ArrayList<DirectoryOption> dirs) {
        super();
        mDirectoryOptions = dirs;
    }

    // DownloadDirectoryProvider implementation.
    @Override
    public void getAllDirectoriesOptions(Callback<ArrayList<DirectoryOption>> callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(mDirectoryOptions));
    }
}
