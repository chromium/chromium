// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Factory for creating instances of the NoteCreationCoordinatorImpl.
 */
public class NoteCreationCoordinatorFactory {
    /**
     * @return a NoteCreationCoordinator instance.
     */
    public static NoteCreationCoordinator create(Activity activity, String selectedText) {
        Profile profile = Profile.getLastUsedRegularProfile();
        return new NoteCreationCoordinatorImpl(
                activity, NoteServiceFactory.getForProfile(profile), selectedText);
    }
}
