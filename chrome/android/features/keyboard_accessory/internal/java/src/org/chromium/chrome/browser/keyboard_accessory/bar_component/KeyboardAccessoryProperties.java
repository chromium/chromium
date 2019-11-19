// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.support.design.widget.TabLayout;
import android.support.v7.widget.RecyclerView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator.TabLayoutCallbacks;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * As model of the keyboard accessory component, this class holds the data relevant to the visual
 * state of the accessory.
 * This includes the visibility of the accessory, its relative position and actions. Whenever the
 * state changes, it notifies its listeners - like the {@link KeyboardAccessoryMediator} or a
 * ModelChangeProcessor.
 */
class KeyboardAccessoryProperties {
    static final ReadableObjectPropertyKey<ListModel<BarItem>> BAR_ITEMS =
            new ReadableObjectPropertyKey<>("bar_items");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableBooleanPropertyKey SKIP_CLOSING_ANIMATION =
            new WritableBooleanPropertyKey("skip_closing_animation");
    static final WritableIntPropertyKey BOTTOM_OFFSET_PX = new WritableIntPropertyKey("offset");
    static final WritableObjectPropertyKey<String> SHEET_TITLE =
            new WritableObjectPropertyKey<>("sheet_title");
    static final WritableBooleanPropertyKey KEYBOARD_TOGGLE_VISIBLE =
            new WritableBooleanPropertyKey("toggle_visible");
    static final WritableObjectPropertyKey<TabLayoutBarItem> TAB_LAYOUT_ITEM =
            new WritableObjectPropertyKey<>("tab_layout_item");
    static final WritableObjectPropertyKey<Runnable> SHOW_KEYBOARD_CALLBACK =
            new WritableObjectPropertyKey<>("keyboard_callback");
    @VisibleForTesting
    static final ReadableBooleanPropertyKey DISABLE_ANIMATIONS_FOR_TESTING =
            new WritableBooleanPropertyKey("skip_all_animations_for_testing");

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel
                .Builder(DISABLE_ANIMATIONS_FOR_TESTING, BAR_ITEMS, VISIBLE, SKIP_CLOSING_ANIMATION,
                        BOTTOM_OFFSET_PX, TAB_LAYOUT_ITEM, KEYBOARD_TOGGLE_VISIBLE, SHEET_TITLE,
                        SHOW_KEYBOARD_CALLBACK)
                .with(BAR_ITEMS, new ListModel<>())
                .with(VISIBLE, false)
                .with(SKIP_CLOSING_ANIMATION, false)
                .with(KEYBOARD_TOGGLE_VISIBLE, false)
                .with(DISABLE_ANIMATIONS_FOR_TESTING, false);
    }

    /**
     * This class wraps data used in ViewHolders of the accessory bar's {@link RecyclerView}.
     * It can hold an {@link Action}s that defines a callback and a recording type.
     */
    static class BarItem {
        /**
         * This type is used to infer which type of view will represent this item.
         */
        @IntDef({Type.ACTION_BUTTON, Type.SUGGESTION, Type.TAB_LAYOUT})
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            int ACTION_BUTTON = 0;
            int SUGGESTION = 1;
            int TAB_LAYOUT = 2;
        }
        private @Type int mType;
        private final @Nullable Action mAction;

        /**
         * Creates a new item. An item must have a type and can have an action.
         * @param type A {@link Type}.
         * @param action An {@link Action}.
         */
        BarItem(@Type int type, @Nullable Action action) {
            mType = type;
            mAction = action;
        }

        /**
         * Returns the which type of view represents this item.
         * @return A {@link Type}.
         */
        @Type
        int getViewType() {
            return mType;
        }

        /**
         * If applicable, returns which action is held by this item.
         * @return An {@link Action}.
         */
        @Nullable
        Action getAction() {
            return mAction;
        }

        @Override
        public String toString() {
            String typeName = "BarItem(" + mType + ")"; // Fallback. We shouldn't crash.
            switch (mType) {
                case Type.ACTION_BUTTON:
                    typeName = "ACTION_BUTTON";
                    break;
                case Type.SUGGESTION:
                    typeName = "SUGGESTION";
                    break;
                case Type.TAB_LAYOUT:
                    typeName = "TAB_LAYOUT";
                    break;
            }
            return typeName + ": " + mAction;
        }
    }
    /**
     * This {@link BarItem} is used to render Autofill suggestions into the accessory bar.
     * For that, it needs (in addition to an {@link Action}) the held {@link AutofillSuggestion}.
     */
    static class AutofillBarItem extends BarItem {
        private final AutofillSuggestion mSuggestion;
        private @Nullable String mFeature;

        /**
         * Creates a new autofill item with a suggestion for the view's representation and an action
         * to handle the interaction with the rendered View.
         * @param suggestion An {@link AutofillSuggestion}.
         * @param action An {@link Action}.
         */
        AutofillBarItem(AutofillSuggestion suggestion, Action action) {
            super(Type.SUGGESTION, action);
            mSuggestion = suggestion;
        }

        AutofillSuggestion getSuggestion() {
            return mSuggestion;
        }

        void setFeatureForIPH(String feature) {
            mFeature = feature;
        }

        void maybeEmitEventForIPH() {
            if (mFeature != null) KeyboardAccessoryIPHUtils.emitFillingEvent(mFeature);
        }

        @Nullable
        String getFeatureForIPH() {
            return mFeature;
        }

        @Override
        public String toString() {
            return "Autofill" + super.toString();
        }
    }

    /**
     * A tab layout in a {@link RecyclerView} can be destroyed and recreated whenever it is
     * scrolled out of/into view. This wrapper allows to trigger a callback whenever the view is
     * recreated so it can be bound to its component.
     */
    static final class TabLayoutBarItem extends BarItem {
        private final TabLayoutCallbacks mTabLayoutCallbacks;

        TabLayoutBarItem(TabLayoutCallbacks tabLayoutCallbacks) {
            super(Type.TAB_LAYOUT, null);
            mTabLayoutCallbacks = tabLayoutCallbacks;
        }

        void notifyAboutViewCreation(TabLayout tabs) {
            mTabLayoutCallbacks.onTabLayoutBound(tabs);
        }

        void notifyAboutViewDestruction(TabLayout tabs) {
            mTabLayoutCallbacks.onTabLayoutUnbound(tabs);
        }
    }

    private KeyboardAccessoryProperties() {}
}
