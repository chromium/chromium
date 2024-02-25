// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter.DownloadLocationHelper;
import org.chromium.chrome.browser.profiles.Profile;

/** Profile aware helper to access and set the default download directory. */
public class DownloadLocationHelperImpl implements DownloadLocationHelper {
    private final Profile mProfile;

    public DownloadLocationHelperImpl(Profile profile) {
        mProfile = profile;
    }

    @Override
    public String getDownloadDefaultDirectory() {
        return DownloadDialogBridge.getDownloadDefaultDirectory(mProfile);
    }

    @Override
    public void setDownloadAndSaveFileDefaultDirectory(String directory) {
        DownloadDialogBridge.setDownloadAndSaveFileDefaultDirectory(mProfile, directory);
    }
}
