// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the view that holds one pane at a time. */
public class HubToolbarViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, HubToolbarView view, PropertyKey key) {
        if (key == ACTION_BUTTON_DATA || key == SHOW_ACTION_BUTTON_TEXT) {
            view.setActionButton(model.get(ACTION_BUTTON_DATA), model.get(SHOW_ACTION_BUTTON_TEXT));
        } else if (key == PANE_SWITCHER_BUTTON_DATA) {
            view.setPaneSwitcherButtonData(
                    model.get(PANE_SWITCHER_BUTTON_DATA), model.get(PANE_SWITCHER_INDEX));
        } else if (key == PANE_SWITCHER_INDEX) {
            view.setPaneSwitcherIndex(model.get(PANE_SWITCHER_INDEX));
        } else if (key == COLOR_SCHEME) {
            view.setColorScheme(model.get(COLOR_SCHEME));
        } else if (key == MENU_BUTTON_VISIBLE) {
            view.setMenuButtonVisible(model.get(MENU_BUTTON_VISIBLE));
        } else if (key == PANE_BUTTON_LOOKUP_CALLBACK) {
            view.setButtonLookupConsumer(model.get(PANE_BUTTON_LOOKUP_CALLBACK));
        } else if (key == SEARCH_BOX_VISIBLE) {
            view.setSearchBoxVisible(model.get(SEARCH_BOX_VISIBLE));
        } else if (key == SEARCH_BOX_LISTENER) {
            view.setSearchBoxListener(model.get(SEARCH_BOX_LISTENER));
        } else if (key == IS_INCOGNITO) {
            view.updateIncognitoElements(model.get(IS_INCOGNITO));
        }
    }
}
