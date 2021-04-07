// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;

/**
 * Parent interface for autofill-assistant dependencies. This interface allows code outside the
 * feature module to access dependencies inside the feature module without leaking the relevant
 * internal types.
 */
public interface AssistantDependencies {
    /**
     * Displays the onboarding to the user.
     *
     * @param useDialogOnboarding whether to show the dialog or bottom-sheet onboarding.
     * @param triggerContext the trigger context to fetch parameters and experiments from.
     * @param callback the callback to invoke with the {@code OnboardingResult}.
     */
    void showOnboarding(
            boolean useDialogOnboarding, TriggerContext triggerContext, Callback<Integer> callback);
}
