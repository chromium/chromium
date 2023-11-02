// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

/**
 * Interface for injecting HelpAndFeedbackLauncher to a fragment. It is useful for modularized
 * fragments that need access to HelpAndFeedbackLauncherImpl.
 */
public interface FragmentHelpAndFeedbackLauncher {
    /**
     * Set an instance of HelpAndFeedbackLauncher in a fragment.
     *
     * @param helpAndFeedbackLauncher The HelpAndFeedbackLauncher that is injected.
     */
    void setHelpAndFeedbackLauncher(HelpAndFeedbackLauncher helpAndFeedbackLauncher);
}
