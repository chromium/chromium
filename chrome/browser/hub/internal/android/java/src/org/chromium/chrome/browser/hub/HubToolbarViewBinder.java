// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the view that holds one pane at a time. */
@NullMarked
public class HubToolbarViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, HubToolbarView view, PropertyKey key) {
        if (key == ACTION_BUTTON_DATA) {
            view.setActionButton(model.get(ACTION_BUTTON_DATA));
        } else if (key == PANE_SWITCHER_BUTTON_DATA) {
            view.setPaneSwitcherButtonData(
                    model.get(PANE_SWITCHER_BUTTON_DATA), model.get(PANE_SWITCHER_INDEX));
        } else if (key == PANE_SWITCHER_INDEX) {
            view.setPaneSwitcherIndex(model.get(PANE_SWITCHER_INDEX));
        } else if (key == COLOR_MIXER) {
            view.setColorMixer(model.get(COLOR_MIXER));
        } else if (key == MENU_BUTTON_VISIBLE) {
            view.setMenuButtonVisible(model.get(MENU_BUTTON_VISIBLE));
        } else if (key == PANE_BUTTON_LOOKUP_CALLBACK) {
            view.setButtonLookupConsumer(model.get(PANE_BUTTON_LOOKUP_CALLBACK));
        } else if (key == SEARCH_BOX_VISIBLE) {
            view.setSearchBoxVisible(model.get(SEARCH_BOX_VISIBLE));
        } else if (key == SEARCH_LOUPE_VISIBLE) {
            view.setSearchLoupeVisible(model.get(SEARCH_LOUPE_VISIBLE));
        } else if (key == SEARCH_LISTENER) {
            view.setSearchListener(model.get(SEARCH_LISTENER));
        } else if (key == IS_INCOGNITO) {
            view.updateIncognitoElements(model.get(IS_INCOGNITO));
        } else if (key == APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION) {
            view.setApplyDelayForSearchBoxAnimation(
                    model.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        } else if (key == HUB_SEARCH_ENABLED_STATE) {
            view.setHubSearchEnabledState(model.get(HUB_SEARCH_ENABLED_STATE));
        }
    }
}
