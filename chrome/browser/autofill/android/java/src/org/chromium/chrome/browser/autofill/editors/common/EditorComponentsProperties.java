// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Common properties for the editor fields or other views displayed in an editor.
 *
 * <p>TODO: crbug.com/476757617 - Split the properties into individual files.
 */
@NullMarked
public class EditorComponentsProperties {
    /** Contains information needed by {@link EditorDialogView} to display fields. */
    public static class EditorItem extends ListItem {
        public final boolean isFullLine;

        public EditorItem(int type, PropertyModel model) {
            this(type, model, /* isFullLine= */ false);
        }

        public EditorItem(int type, PropertyModel model, boolean isFullLine) {
            super(type, model);
            this.isFullLine = isFullLine;
        }
    }

    /*
     * Types of fields this editor model supports.
     */
    @IntDef({
        ItemType.DROPDOWN,
        ItemType.TEXT_INPUT,
        ItemType.NON_EDITABLE_TEXT,
        ItemType.NOTICE,
        ItemType.DATE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        // A fixed list of values, only 1 of which can be selected.
        int DROPDOWN = 1;
        // User can fill in a sequence of characters subject to input type restrictions.
        int TEXT_INPUT = 2;
        // A non-editable constant string.
        int NON_EDITABLE_TEXT = 3;
        // A notice string that is not editable.
        int NOTICE = 4;
        // 3 dropdowns to pick a date.
        int DATE = 5;
    }

    public static boolean isDropdownField(ListItem fieldItem) {
        return fieldItem.type == ItemType.DROPDOWN;
    }

    public static boolean isEditable(ListItem fieldItem) {
        return fieldItem.type == ItemType.DROPDOWN || fieldItem.type == ItemType.TEXT_INPUT;
    }

    /** Properties specific for the non-editable text fields. */
    public static class NonEditableTextProperties {
        public static final ReadableObjectPropertyKey<String> PRIMARY_TEXT =
                new ReadableObjectPropertyKey<>("text");
        public static final ReadableObjectPropertyKey<String> SECONDARY_TEXT =
                new ReadableObjectPropertyKey<>("secondary_text");
        public static final ReadableObjectPropertyKey<Runnable> CLICK_RUNNABLE =
                new ReadableObjectPropertyKey<>("click_runnable");
        public static final ReadableIntPropertyKey ICON = new ReadableIntPropertyKey("icon");
        public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
                new ReadableObjectPropertyKey<>("content_description");

        public static final PropertyKey[] NON_EDITABLE_TEXT_ALL_KEYS = {
            PRIMARY_TEXT, SECONDARY_TEXT, CLICK_RUNNABLE, ICON, CONTENT_DESCRIPTION
        };
    }

    /** Properties specific for the notice fields. */
    public static class NoticeProperties {
        public static final ReadableObjectPropertyKey<String> NOTICE_TEXT =
                new ReadableObjectPropertyKey<>("notice_text");

        public static final ReadableBooleanPropertyKey SHOW_BACKGROUND =
                new ReadableBooleanPropertyKey("show_background");

        public static final ReadableBooleanPropertyKey IMPORTANT_FOR_ACCESSIBILITY =
                new ReadableBooleanPropertyKey("important_for_accessibility");

        public static final PropertyKey[] NOTICE_ALL_KEYS = {
            NOTICE_TEXT, SHOW_BACKGROUND, IMPORTANT_FOR_ACCESSIBILITY
        };
    }
}
