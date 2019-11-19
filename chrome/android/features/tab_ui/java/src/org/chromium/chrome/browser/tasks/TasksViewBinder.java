// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the tasks surface view.
class TasksViewBinder {
    public static void bind(PropertyModel model, TasksView view, PropertyKey propertyKey) {
        if (propertyKey == FAKE_SEARCH_BOX_CLICK_LISTENER) {
            view.setFakeSearchBoxClickListener(model.get(FAKE_SEARCH_BOX_CLICK_LISTENER));
        } else if (propertyKey == FAKE_SEARCH_BOX_TEXT_WATCHER) {
            view.setFakeSearchBoxTextWatcher(model.get(FAKE_SEARCH_BOX_TEXT_WATCHER));
        } else if (propertyKey == IS_FAKE_SEARCH_BOX_VISIBLE) {
            view.setFakeSearchBoxVisibility(model.get(IS_FAKE_SEARCH_BOX_VISIBLE));
        } else if (propertyKey == IS_INCOGNITO) {
            view.setIncognitoMode(model.get(IS_INCOGNITO));
        } else if (propertyKey == IS_TAB_CAROUSEL_VISIBLE) {
            view.setTabCarouselVisibility(model.get(IS_TAB_CAROUSEL_VISIBLE));
        } else if (propertyKey == IS_VOICE_RECOGNITION_BUTTON_VISIBLE) {
            view.setVoiceRecognitionButtonVisibility(
                    model.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE));
        } else if (propertyKey == MORE_TABS_CLICK_LISTENER) {
            view.setMoreTabsOnClickListener(model.get(MORE_TABS_CLICK_LISTENER));
        } else if (propertyKey == MV_TILES_VISIBLE) {
            view.setMostVisitedVisibility(model.get(MV_TILES_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == VOICE_SEARCH_BUTTON_CLICK_LISTENER) {
            view.setVoiceRecognitionButtonClickListener(
                    model.get(VOICE_SEARCH_BUTTON_CLICK_LISTENER));
        }
    }
}
