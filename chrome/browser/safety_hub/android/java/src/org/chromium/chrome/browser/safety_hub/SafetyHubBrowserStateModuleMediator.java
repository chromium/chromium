// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.components.browser_ui.settings.CardPreference;

import java.util.List;

/**
 * Mediator for the Safety Hub browser state module. Populates the {@link CardPreference} with the
 * user's browser state, which is based in all the other modules.
 */
@NullMarked
public class SafetyHubBrowserStateModuleMediator {
    private final CardPreference mPreference;
    private final List<SafetyHubModuleMediator> mModuleMediators;

    SafetyHubBrowserStateModuleMediator(
            CardPreference preference, List<SafetyHubModuleMediator> moduleMediators) {
        mPreference = preference;
        mModuleMediators = moduleMediators;
    }

    public void setUpModule() {
        mPreference.setTitle(R.string.safety_hub_safe_browser_state_title);
        mPreference.setSummary(
                mPreference.getContext().getString(R.string.safety_hub_checked_recently));
        mPreference.setIconDrawable(
                AppCompatResources.getDrawable(
                        mPreference.getContext(), R.drawable.ic_check_circle_filled_green_24dp));
        mPreference.setShouldCenterIcon(true);
        mPreference.setCloseIconVisibility(View.GONE);
        mPreference.setVisible(false);
    }

    public void destroy() {
        mModuleMediators.clear();
    }

    public void updateModule() {
        mPreference.setVisible(isBrowserStateSafe());
    }

    public boolean isBrowserStateSafe() {
        for (SafetyHubModuleMediator mediator : mModuleMediators) {
            if (mediator.getModuleState() != ModuleState.SAFE
                    && mediator.getModuleState() != ModuleState.INFO) {
                return false;
            }
        }
        return true;
    }
}
