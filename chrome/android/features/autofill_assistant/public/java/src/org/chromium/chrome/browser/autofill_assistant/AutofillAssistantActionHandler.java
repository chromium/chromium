// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Interface that provides implementation for AA actions, triggered by direct actions.
 */
public interface AutofillAssistantActionHandler {
    /**
     * Start fetching potential actions for websites.
     *
     * <p>This method starts AA on the current tab, if necessary, and waits for the first results.
     * The method hasRunFirstCheck will return true when that's done. Note that {@code callback}
     * will receive true if fetching actions was successful and the resulting set is not empty.
     * <p>This method needs to be called before getActions below will return anything.
     *
     * @param userName name of the user to use when sending RPCs. Might be empty.
     * @param experimentIds comma-separated set of experiment ids. Might be empty
     * @param arguments extra arguments to include into the RPC. Might be empty.
     * @param callback callback to report when actions are available.
     */
    void fetchWebsiteActions(
            String userName, String experimentIds, Bundle arguments, Callback<Boolean> callback);

    /**
     * Returns if autofill assistant has fetched actions for the current URL.
     *
     * <p>This method queries the underlying Autofill Assistant stack to find out if it has been
     * started and whether the first round of fetching scripts and checking availability has been
     * completed.
     *
     * @return Whether scripts have been fetched and preconditions checked.
     */
    boolean hasRunFirstCheck();

    /**
     * Returns the available AA actions to be reported to the direct actions framework.
     *
     * <p>This method simply returns the list of actions known to AA. An empty string array means
     * either that the controller has not yet been started or there are no actions available for the
     * current website.
     *
     * @return Array of actions containing the names and arguments of known actions.
     */
    List<AutofillAssistantDirectAction> getActions();

    /** Performs onboarding and returns the result to the callback. */
    void performOnboarding(String experimentIds, Callback<Boolean> callback);

    /**
     * Performs an AA action.
     *
     * <p>If this method returns {@code true}, a definition for the action was successfully started.
     * It can still fail later, and the failure will be reported to the UI.
     *
     * @param name action name, might be empty to autostart
     * @param experimentIds comma-separated set of experiment ids. Might be empty.
     * @param arguments extra arguments to pass to the action. Might be empty.
     * @param callback to report the result to
     */
    void performAction(
            String name, String experimentIds, Bundle arguments, Callback<Boolean> callback);
}
