// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.feature_engagement.Tracker;
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
    static final WritableObjectPropertyKey<SheetOpenerBarItem> SHEET_OPENER_ITEM =
            new WritableObjectPropertyKey<>("sheet_opener_item");
    static final ReadableBooleanPropertyKey DISABLE_ANIMATIONS_FOR_TESTING =
            new ReadableBooleanPropertyKey("skip_all_animations_for_testing");
    static final WritableObjectPropertyKey<Callback<Integer>> OBFUSCATED_CHILD_AT_CALLBACK =
            new WritableObjectPropertyKey<>("obfuscated_child_at_callback");
    static final PropertyModel.WritableObjectPropertyKey<Callback<Boolean>>
            ON_TOUCH_EVENT_CALLBACK =
                    new PropertyModel.WritableObjectPropertyKey<>("on_touch_event_handler");
    static final WritableBooleanPropertyKey SHOW_SWIPING_IPH =
            new WritableBooleanPropertyKey("show_swiping_iph");
    static final WritableBooleanPropertyKey HAS_SUGGESTIONS =
            new WritableBooleanPropertyKey("has_suggestions");

    static final WritableObjectPropertyKey<KeyboardAccessoryView.AnimationListener>
            ANIMATION_LISTENER = new WritableObjectPropertyKey<>("animation_listener");

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(
                        DISABLE_ANIMATIONS_FOR_TESTING,
                        BAR_ITEMS,
                        VISIBLE,
                        SKIP_CLOSING_ANIMATION,
                        BOTTOM_OFFSET_PX,
                        SHEET_OPENER_ITEM,
                        OBFUSCATED_CHILD_AT_CALLBACK,
                        ON_TOUCH_EVENT_CALLBACK,
                        SHOW_SWIPING_IPH,
                        HAS_SUGGESTIONS,
                        ANIMATION_LISTENER)
                .with(BAR_ITEMS, new ListModel<>())
                .with(VISIBLE, false)
                .with(SKIP_CLOSING_ANIMATION, false)
                .with(DISABLE_ANIMATIONS_FOR_TESTING, false)
                .with(SHOW_SWIPING_IPH, false)
                .with(HAS_SUGGESTIONS, false);
    }

    /**
     * This class wraps data used in ViewHolders of the accessory bar's {@link RecyclerView}.
     * It can hold an {@link Action}s that defines a callback and a recording type.
     */
    static class BarItem {
        /** This type is used to infer which type of view will represent this item. */
        @IntDef({Type.ACTION_BUTTON, Type.SUGGESTION, Type.TAB_LAYOUT, Type.ACTION_CHIP})
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            int ACTION_BUTTON = 0;
            int SUGGESTION = 1;
            int TAB_LAYOUT = 2;
            int ACTION_CHIP = 3;
        }

        private @Type int mType;
        private final @Nullable Action mAction;
        private final @StringRes int mCaptionId;

        /**
         * Creates a new item. An item must have a type and can have an action.
         *
         * @param type A {@link Type}.
         * @param action An {@link Action}.
         * @param captionId A {@link StringRes} to describe the bar item.
         */
        BarItem(@Type int type, @Nullable Action action, @StringRes int captionId) {
            mType = type;
            mAction = action;
            mCaptionId = captionId;
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

        /**
         * If applicable, returns the caption id of this bar item.
         *
         * @return A {@link StringRes}.
         */
        @StringRes
        int getCaptionId() {
            return mCaptionId;
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
                case Type.ACTION_CHIP:
                    typeName = "ACTION_CHIP";
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
            super(Type.SUGGESTION, action, 0);
            mSuggestion = suggestion;
        }

        AutofillSuggestion getSuggestion() {
            return mSuggestion;
        }

        void setFeatureForIPH(String feature) {
            mFeature = feature;
        }

        void maybeEmitEventForIPH(Tracker tracker) {
            if (mFeature != null) KeyboardAccessoryIPHUtils.emitFillingEvent(tracker, mFeature);
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
     * A tab layout or a button group in a {@link RecyclerView} can be destroyed and recreated
     * whenever it is scrolled out of/into view. This wrapper allows to trigger a callback whenever
     * the view is recreated so it can be bound to its component.
     */
    static final class SheetOpenerBarItem extends BarItem {
        private final SheetOpenerCallbacks mSheetOpenerCallbacks;

        SheetOpenerBarItem(SheetOpenerCallbacks sheetOpenerCallbacks) {
            super(Type.TAB_LAYOUT, null, 0);
            mSheetOpenerCallbacks = sheetOpenerCallbacks;
        }

        void notifyAboutViewCreation(View view) {
            mSheetOpenerCallbacks.onViewBound(view);
        }

        void notifyAboutViewDestruction(View view) {
            mSheetOpenerCallbacks.onViewUnbound(view);
        }
    }

    private KeyboardAccessoryProperties() {}
}
