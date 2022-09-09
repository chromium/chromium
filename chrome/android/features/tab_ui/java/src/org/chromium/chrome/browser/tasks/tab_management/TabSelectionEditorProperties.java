// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link PropertyKey} list for TabSelectionEditor.
 */
public class TabSelectionEditorProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_ACTION_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<String> TOOLBAR_ACTION_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel
            .WritableIntPropertyKey TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> TOOLBAR_NAVIGATION_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey TOOLBAR_BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<ColorStateList> TOOLBAR_GROUP_TEXT_TINT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel
            .WritableObjectPropertyKey<ColorStateList> TOOLBAR_GROUP_BUTTON_TINT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel
            .WritableIntPropertyKey TOOLBAR_ACTION_BUTTON_DESCRIPTION_RESOURCE_ID =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey TOOLBAR_ACTION_BUTTON_VISIBILITY =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<TabSelectionEditorToolbar.RelatedTabCountProvider>
                    RELATED_TAB_COUNT_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_VISIBLE,
            TOOLBAR_ACTION_BUTTON_LISTENER, TOOLBAR_ACTION_BUTTON_TEXT,
            TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, TOOLBAR_NAVIGATION_LISTENER, PRIMARY_COLOR,
            TOOLBAR_BACKGROUND_COLOR, TOOLBAR_GROUP_TEXT_TINT, TOOLBAR_GROUP_BUTTON_TINT,
            TOOLBAR_ACTION_BUTTON_DESCRIPTION_RESOURCE_ID, TOOLBAR_ACTION_BUTTON_VISIBILITY,
            RELATED_TAB_COUNT_PROVIDER};
}
