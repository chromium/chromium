// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;

/** Factory for {@link HelpAndFeedbackLauncher}. Can be used from chrome/browser modules. */
public class HelpAndFeedbackLauncherFactory {
    private static HelpAndFeedbackLauncher sInstanceForTesting;

    /** Get a {@link HelpAndFeedbackLauncher} for the given {@link Profile}. */
    public static HelpAndFeedbackLauncher getForProfile(Profile profile) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return HelpAndFeedbackLauncherImpl.getForProfile(profile);
    }

    /** Set a test double to replace the real {@link HelpAndFeedbackLauncherImpl} in a test. */
    public static void setInstanceForTesting(HelpAndFeedbackLauncher instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
