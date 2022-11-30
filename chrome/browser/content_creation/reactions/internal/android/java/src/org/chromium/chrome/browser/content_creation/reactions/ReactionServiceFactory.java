// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.content_creation.reactions.ReactionService;

/**
 * Basic factory that creates and returns a {@link ReactionService} that is
 * attached natively to the given {@link Profile}.
 */
public class ReactionServiceFactory {
    /**
     * Used to get access to the reaction service backend.
     */
    public static ReactionService getForProfile(Profile profile) {
        return ReactionServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        ReactionService getForProfile(Profile profile);
    }
}