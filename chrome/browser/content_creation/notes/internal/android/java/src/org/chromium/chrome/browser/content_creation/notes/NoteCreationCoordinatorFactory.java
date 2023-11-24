// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;

/** Factory for creating instances of the NoteCreationCoordinatorImpl. */
public class NoteCreationCoordinatorFactory {
    /**
     * @return a NoteCreationCoordinator instance.
     */
    public static NoteCreationCoordinator create(
            Activity activity,
            WindowAndroid windowAndroid,
            String shareUrl,
            String title,
            String selectedText,
            ChromeOptionShareCallback chromeOptionShareCallback) {
        Profile profile = Profile.getLastUsedRegularProfile();
        return new NoteCreationCoordinatorImpl(
                activity,
                windowAndroid,
                NoteServiceFactory.getForProfile(profile),
                chromeOptionShareCallback,
                shareUrl,
                title,
                selectedText);
    }
}
