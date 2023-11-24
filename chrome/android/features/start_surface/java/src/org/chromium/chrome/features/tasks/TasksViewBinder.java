// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_MANAGER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_LEARN_MORE_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.LENS_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.QUERY_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.RESET_TASK_SURFACE_HEADER_SCROLL_POSITION;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.SINGLE_TAB_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TOP_TOOLBAR_PLACEHOLDER_HEIGHT;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder of the tasks surface view. */
public class TasksViewBinder {
    public static void bind(PropertyModel model, TasksView view, PropertyKey propertyKey) {
        if (propertyKey == FAKE_SEARCH_BOX_CLICK_LISTENER) {
            view.getSearchBoxCoordinator()
                    .setSearchBoxClickListener(model.get(FAKE_SEARCH_BOX_CLICK_LISTENER));
        } else if (propertyKey == FAKE_SEARCH_BOX_TEXT_WATCHER) {
            view.getSearchBoxCoordinator()
                    .setSearchBoxTextWatcher(model.get(FAKE_SEARCH_BOX_TEXT_WATCHER));
        } else if (propertyKey == INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER) {
            view.setIncognitoCookieControlsIconClickListener(
                    model.get(INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER));
        } else if (propertyKey == INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED) {
            view.setIncognitoCookieControlsToggleChecked(
                    model.get(INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED));
        } else if (propertyKey == INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER) {
            view.setIncognitoCookieControlsToggleCheckedListener(
                    model.get(INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER));
        } else if (propertyKey == INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT) {
            view.setIncognitoCookieControlsToggleEnforcement(
                    model.get(INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT));
        } else if (propertyKey == INCOGNITO_LEARN_MORE_CLICK_LISTENER) {
            view.setIncognitoDescriptionLearnMoreClickListener(
                    model.get(INCOGNITO_LEARN_MORE_CLICK_LISTENER));
        } else if (propertyKey == IS_FAKE_SEARCH_BOX_VISIBLE) {
            view.getSearchBoxCoordinator().setVisibility(model.get(IS_FAKE_SEARCH_BOX_VISIBLE));
        } else if (propertyKey == IS_INCOGNITO) {
            view.setIncognitoMode(model.get(IS_INCOGNITO));
        } else if (propertyKey == IS_INCOGNITO_DESCRIPTION_INITIALIZED) {
            if (model.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED)) {
                view.initializeIncognitoDescriptionView();
            }
        } else if (propertyKey == IS_INCOGNITO_DESCRIPTION_VISIBLE) {
            boolean isVisible = model.get(IS_INCOGNITO_DESCRIPTION_VISIBLE);
            if (isVisible) {
                // Need to update the service when the description view becomes visible because
                // there may be a new off the record profile.
                model.get(INCOGNITO_COOKIE_CONTROLS_MANAGER).updateIfNecessary();
            }
            view.setIncognitoDescriptionVisibility(isVisible);
        } else if (propertyKey == IS_LENS_BUTTON_VISIBLE) {
            view.getSearchBoxCoordinator()
                    .setLensButtonVisibility(model.get(IS_LENS_BUTTON_VISIBLE));
        } else if (propertyKey == IS_SURFACE_BODY_VISIBLE) {
            view.setSurfaceBodyVisibility(model.get(IS_SURFACE_BODY_VISIBLE));
        } else if (propertyKey == IS_TAB_CAROUSEL_VISIBLE) {
            view.setTabCarouselVisibility(model.get(IS_TAB_CAROUSEL_VISIBLE));
        } else if (propertyKey == IS_TAB_CAROUSEL_TITLE_VISIBLE) {
            view.setTabCarouselTitleVisibility(model.get(IS_TAB_CAROUSEL_TITLE_VISIBLE));
        } else if (propertyKey == IS_VOICE_RECOGNITION_BUTTON_VISIBLE) {
            view.getSearchBoxCoordinator()
                    .setVoiceSearchButtonVisibility(model.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE));
        } else if (propertyKey == LENS_BUTTON_CLICK_LISTENER) {
            view.getSearchBoxCoordinator()
                    .addLensButtonClickListener(model.get(LENS_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == MORE_TABS_CLICK_LISTENER) {
            view.setMoreTabsOnClickListener(model.get(MORE_TABS_CLICK_LISTENER));
        } else if (propertyKey == MV_TILES_VISIBLE) {
            view.setMostVisitedVisibility(model.get(MV_TILES_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == QUERY_TILES_VISIBLE) {
            view.setQueryTilesVisibility(model.get(QUERY_TILES_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == VOICE_SEARCH_BUTTON_CLICK_LISTENER) {
            view.getSearchBoxCoordinator()
                    .addVoiceSearchButtonClickListener(
                            model.get(VOICE_SEARCH_BUTTON_CLICK_LISTENER));
        } else if (propertyKey == TASKS_SURFACE_BODY_TOP_MARGIN) {
            view.setTasksSurfaceBodyTopMargin(model.get(TASKS_SURFACE_BODY_TOP_MARGIN));
        } else if (propertyKey == MV_TILES_CONTAINER_TOP_MARGIN) {
            view.setMVTilesContainerTopMargin(model.get(MV_TILES_CONTAINER_TOP_MARGIN));
        } else if (propertyKey == MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN) {
            view.setMVTilesContainerLeftAndRightMargin(
                    model.get(MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN));
        } else if (propertyKey == TAB_SWITCHER_TITLE_TOP_MARGIN) {
            view.setTabSwitcherTitleTopMargin(model.get(TAB_SWITCHER_TITLE_TOP_MARGIN));
        } else if (propertyKey == SINGLE_TAB_TOP_MARGIN) {
            view.setSingleTabTopMargin(model.get(SINGLE_TAB_TOP_MARGIN));
        } else if (propertyKey == RESET_TASK_SURFACE_HEADER_SCROLL_POSITION) {
            view.resetScrollPosition();
        } else if (propertyKey == TOP_TOOLBAR_PLACEHOLDER_HEIGHT) {
            view.setTopToolbarPlaceholderHeight(model.get(TOP_TOOLBAR_PLACEHOLDER_HEIGHT));
        } else if (propertyKey == BACKGROUND_COLOR) {
            view.setStartSurfaceBackgroundColor(model.get(BACKGROUND_COLOR));
        }
    }
}
