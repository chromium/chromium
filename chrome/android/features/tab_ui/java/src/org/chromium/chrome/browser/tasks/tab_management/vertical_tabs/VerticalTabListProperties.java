// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Properties for the Vertical Tab List. */
@NullMarked
public class VerticalTabListProperties {
    /** State of the Vertical Tab Rail layout. */
    @IntDef({
        RailCollapseState.EXPANDED,
        RailCollapseState.COLLAPSED,
        RailCollapseState.EXPANDED_FOR_HOVERING
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE})
    public @interface RailCollapseState {
        /** The rail is fully expanded, showing tab favicons and titles. */
        int EXPANDED = 0;

        /** The rail is collapsed, showing only tab favicons. */
        int COLLAPSED = 1;

        /** The rail is temporarily expanded (e.g. during hover), overlaying content. */
        int EXPANDED_FOR_HOVERING = 2;
    }

    public static final PropertyModel.WritableIntPropertyKey COLLAPSE_STATE =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_COLLAPSE_BUTTON_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_GRID_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_SEARCH_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_NEW_TAB_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_COLLAPSE_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
                    Callback<@RailCollapseState Integer>>
            EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                COLLAPSE_STATE,
                IS_COLLAPSE_BUTTON_ENABLED,
                IS_INCOGNITO,
                ON_GRID_CLICK_LISTENER,
                ON_SEARCH_CLICK_LISTENER,
                ON_NEW_TAB_CLICK_LISTENER,
                ON_COLLAPSE_CLICK_LISTENER,
                EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER
            };
}
