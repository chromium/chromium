// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.IncognitoSwitchProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.IncognitoSwitchProperties.IS_VISIBLE;

import android.view.View;
import android.widget.Switch;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for pushing updates to the Android view of the incognito switch. These
 * updates are pulled from the {@link IncognitoSwitchProperties} when a notification of an update is
 * received.
 */
class IncognitoSwitchViewBinder {
    /**
     * Build a binder that handles interaction between the model and the view that make up the
     * incognito switch.
     */
    public static void bind(PropertyModel model, Switch incognitoSwitch, PropertyKey propertyKey) {
        if (IncognitoSwitchProperties.ON_CHECKED_CHANGE_LISTENER == propertyKey) {
            incognitoSwitch.setOnCheckedChangeListener(
                    model.get(IncognitoSwitchProperties.ON_CHECKED_CHANGE_LISTENER));
        } else if (IS_INCOGNITO == propertyKey) {
            boolean isIncognito = model.get(IS_INCOGNITO);
            incognitoSwitch.setChecked(isIncognito);

            final int stackAnnouncementId = isIncognito
                    ? R.string.accessibility_tab_switcher_incognito_stack_selected
                    : R.string.accessibility_tab_switcher_standard_stack_selected;
            incognitoSwitch.announceForAccessibility(
                    incognitoSwitch.getResources().getString(stackAnnouncementId));
            final int descriptionResId = isIncognito
                    ? R.string.accessibility_tabstrip_btn_incognito_toggle_incognito
                    : R.string.accessibility_tabstrip_btn_incognito_toggle_standard;
            incognitoSwitch.setContentDescription(
                    incognitoSwitch.getResources().getString(descriptionResId));
        } else if (IS_VISIBLE == propertyKey) {
            incognitoSwitch.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        }
    }
}
