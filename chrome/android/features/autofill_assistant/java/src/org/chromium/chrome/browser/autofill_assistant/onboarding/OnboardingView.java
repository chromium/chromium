// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.WebContents;

/**
 * Interface for a Java-side autofill assistant onboarding coordinator.
 */
public interface OnboardingView {
    /**
     * Shows onboarding and provides the result to the given callback.
     *
     * <p>The {@code callback} will be called when the user accepts, cancels or dismisses the
     * onboarding.
     *
     * <p>The {@code targetUrl} is the initial URL Autofill Assistant is being started on. The
     * navigation to that URL is allowed, other navigations will hide Autofill Assistant.
     *
     * @param callback Callback to report when user accepts or cancels the onboarding.
     * @param webContents WebContents java wrapper to allow communication with the native
     *         WebContents object.
     * @param targetUrl The initial URL Autofill Assistant is being started on.
     */
    void show(Callback<Integer> callback, WebContents webContents, String targetUrl);

    /** Hides the onboarding UI, if one is shown. */
    void hide();

    /**
     * Returns {@code true} if the onboarding has been shown at the beginning when this
     * autofill assistant flow got triggered.
     *
     * @return Whether the onboarding screen has been shown to the user.
     */
    boolean getOnboardingShown();
}