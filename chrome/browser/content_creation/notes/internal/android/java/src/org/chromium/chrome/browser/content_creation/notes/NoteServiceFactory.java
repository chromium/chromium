// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.content_creation.notes.NoteService;

/**
 * Basic factory that creates and returns a {@link NoteService} that is
 * attached natively to the given {@link Profile}.
 */
public class NoteServiceFactory {
    /**
     * Used to get access to the note service backend.
     */
    public static NoteService getForProfile(Profile profile) {
        return NoteServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        NoteService getForProfile(Profile profile);
    }
}