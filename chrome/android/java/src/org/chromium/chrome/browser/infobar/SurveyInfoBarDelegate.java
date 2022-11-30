// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * Delegate for survey info bar actions.
 */
public interface SurveyInfoBarDelegate {
    /**
     * Notified when the tab containing the infobar is closed.
     */
    void onSurveyInfoBarTabHidden();

    /**
     * Notified when the interactability of the tab containing the infobar is changed.
     */
    void onSurveyInfoBarTabInteractabilityChanged(boolean isInteractable);

    /**
     * Notified when the survey infobar is closed.
     * @param viaCloseButton If the infobar's close button was tapped to close the infobar.
     * @param visibleWhenClosed If the infobar was visible when closed (i.e. not hidden behind
     *                          another infobar).
     */
    void onSurveyInfoBarClosed(boolean viaCloseButton, boolean visibleWhenClosed);

    /**
     * Notified when the survey is triggered via the infobar.
     */
    void onSurveyTriggered();

    /**
     * Called to supply the survey info bar with the prompt string.
     * @return The string that will be displayed on the info bar.
     */
    String getSurveyPromptString();

    /**
     * Called to supply the survey info bar with lifecycle dispatcher used to show survey.
     * @return The lifecycle dispatcher used to dispatch signals from the activity.
     * */
    ActivityLifecycleDispatcher getLifecycleDispatcher();
}
