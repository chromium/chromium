// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.utils.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.AutofillProfilePayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.FillingProduct;
import org.chromium.components.autofill.FillingProductBridge;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;

/**
 * As model of the keyboard accessory component, this class holds the data relevant to the visual
 * state of the accessory. This includes the visibility of the accessory, its relative position and
 * actions. Whenever the state changes, it notifies its listeners - like the {@link
 * KeyboardAccessoryMediator} or a ModelChangeProcessor.
 */
@NullMarked
class KeyboardAccessoryProperties {
    static final ReadableObjectPropertyKey<ListModel<BarItem>> BAR_ITEMS_FIXED =
            new ReadableObjectPropertyKey<>("bar_items_fixed");
    static final ReadableObjectPropertyKey<ListModel<BarItem>> BAR_ITEMS =
            new ReadableObjectPropertyKey<>("bar_items");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableBooleanPropertyKey SKIP_CLOSING_ANIMATION =
            new WritableBooleanPropertyKey("skip_closing_animation");
    static final WritableObjectPropertyKey<SheetOpenerBarItem> SHEET_OPENER_ITEM =
            new WritableObjectPropertyKey<>("sheet_opener_item");
    static final WritableObjectPropertyKey<DismissBarItem> DISMISS_ITEM =
            new WritableObjectPropertyKey<>("dismiss_item");
    static final ReadableBooleanPropertyKey DISABLE_ANIMATIONS_FOR_TESTING =
            new ReadableBooleanPropertyKey("skip_all_animations_for_testing");
    static final WritableObjectPropertyKey<KeyboardAccessoryStyle> STYLE =
            new WritableObjectPropertyKey<>("style");
    static final WritableObjectPropertyKey<Callback<Integer>> OBFUSCATED_CHILD_AT_CALLBACK =
            new WritableObjectPropertyKey<>("obfuscated_child_at_callback");
    static final PropertyModel.WritableObjectPropertyKey<Callback<Boolean>>
            ON_TOUCH_EVENT_CALLBACK =
                    new PropertyModel.WritableObjectPropertyKey<>("on_touch_event_handler");
    static final WritableBooleanPropertyKey SHOW_SWIPING_IPH =
            new WritableBooleanPropertyKey("show_swiping_iph");
    static final WritableBooleanPropertyKey HAS_SUGGESTIONS =
            new WritableBooleanPropertyKey("has_suggestions");
    static final WritableBooleanPropertyKey HAS_STICKY_LAST_ITEM =
            new WritableBooleanPropertyKey("has_sticky_last_item");
    static final WritableBooleanPropertyKey ANIMATE_SUGGESTIONS_FROM_TOP =
            new WritableBooleanPropertyKey("animate_suggestions_from_top");

    static final WritableObjectPropertyKey<KeyboardAccessoryView.AnimationListener>
            ANIMATION_LISTENER = new WritableObjectPropertyKey<>("animation_listener");

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(
                        DISABLE_ANIMATIONS_FOR_TESTING,
                        BAR_ITEMS_FIXED,
                        BAR_ITEMS,
                        VISIBLE,
                        SKIP_CLOSING_ANIMATION,
                        STYLE,
                        SHEET_OPENER_ITEM,
                        DISMISS_ITEM,
                        OBFUSCATED_CHILD_AT_CALLBACK,
                        ON_TOUCH_EVENT_CALLBACK,
                        SHOW_SWIPING_IPH,
                        HAS_SUGGESTIONS,
                        HAS_STICKY_LAST_ITEM,
                        ANIMATE_SUGGESTIONS_FROM_TOP,
                        ANIMATION_LISTENER)
                .with(BAR_ITEMS_FIXED, new ListModel<>())
                .with(BAR_ITEMS, new ListModel<>())
                .with(VISIBLE, false)
                .with(SKIP_CLOSING_ANIMATION, false)
                .with(DISABLE_ANIMATIONS_FOR_TESTING, false)
                .with(SHOW_SWIPING_IPH, false)
                .with(HAS_SUGGESTIONS, false)
                .with(ANIMATE_SUGGESTIONS_FROM_TOP, false);
    }

    /** This class wraps 1 or several items displayed in the KeyboardAccessory. */
    abstract static class BarItem {
        /** This type is used to infer which type of view will represent this item. */
        @IntDef({
            Type.ACTION_BUTTON,
            Type.SUGGESTION,
            Type.LOYALTY_CARD_SUGGESTION,
            Type.HOME_AND_WORK_SUGGESTION,
            Type.TAB_LAYOUT,
            Type.ACTION_CHIP,
            Type.DISMISS_CHIP,
            Type.GROUP,
            Type.PAYMENTS_SUGGESTION
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            int ACTION_BUTTON = 0;
            int SUGGESTION = 1;
            int LOYALTY_CARD_SUGGESTION = 2;
            int HOME_AND_WORK_SUGGESTION = 3;
            int TAB_LAYOUT = 4;
            int ACTION_CHIP = 5;
            int DISMISS_CHIP = 6;
            int GROUP = 7;
            int PAYMENTS_SUGGESTION = 8;
        }

        private final @Type int mType;

        /**
         * Creates a new item. An item must have a type.
         *
         * @param type A {@link Type}.
         */
        BarItem(@Type int type) {
            mType = type;
        }

        /**
         * Returns the which type of view represents this item.
         *
         * @return A {@link Type}.
         */
        @Type
        int getViewType() {
            return mType;
        }

        /**
         * If this {@link BarItem} is a instance of {@link ActionBarItem}, returns itself in a list.
         * Otherwise, returns a list of {@link ActionBarItem} contained in this group.
         */
        abstract List<ActionBarItem> getActionBarItems();
    }

    /**
     * This class groups a couple of {@link AutofillBarItem} for UI purposes. For more information,
     * see {@link KeyboardAccessoryChipGroup}.
     */
    static class GroupBarItem extends BarItem {
        private final List<ActionBarItem> mActionBarItems;

        GroupBarItem(List<ActionBarItem> actionBarItems) {
            super(Type.GROUP);
            mActionBarItems = actionBarItems;
        }

        @Override
        List<ActionBarItem> getActionBarItems() {
            return Collections.unmodifiableList(mActionBarItems);
        }
    }

    /**
     * This class wraps data used in ViewHolders of the accessory bar's {@link RecyclerView}. It can
     * hold an {@link Action}s that defines a callback and a recording type.
     */
    static class ActionBarItem extends BarItem {
        private final @Nullable Action mAction;
        private final @StringRes int mCaptionId;

        /**
         * Creates a new item. An action item must have a type and can have an action.
         *
         * @param type A {@link Type}.
         * @param action An {@link Action}.
         * @param captionId A {@link StringRes} to describe the bar item.
         */
        ActionBarItem(@Type int type, @Nullable Action action, @StringRes int captionId) {
            super(type);
            mAction = action;
            mCaptionId = captionId;
        }

        @Override
        List<ActionBarItem> getActionBarItems() {
            return List.of(this);
        }

        /**
         * If applicable, returns which action is held by this item.
         *
         * @return An {@link Action}.
         */
        @Nullable Action getAction() {
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
            String typeName = "BarItem(" + getViewType() + ")"; // Fallback. We shouldn't crash.
            switch (getViewType()) {
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
                case Type.DISMISS_CHIP:
                    typeName = "DISMISS_CHIP";
                    break;
            }
            return typeName + ": " + mAction;
        }
    }

    /**
     * This {@link BarItem} is used to render Autofill suggestions into the accessory bar. For that,
     * it needs (in addition to an {@link Action}) the held {@link AutofillSuggestion}.
     */
    static class AutofillBarItem extends ActionBarItem {
        private final AutofillSuggestion mSuggestion;
        private @Nullable String mFeature;

        /**
         * Creates a new autofill item with a suggestion for the view's representation and an action
         * to handle the interaction with the rendered View.
         *
         * @param suggestion An {@link AutofillSuggestion}.
         * @param action An {@link Action}.
         * @param profile The {@link Profile} associated with the autofill data.
         */
        AutofillBarItem(AutofillSuggestion suggestion, Action action, Profile profile) {
            super(getBarItemType(suggestion, profile), action, 0);
            mSuggestion = suggestion;
        }

        AutofillSuggestion getSuggestion() {
            return mSuggestion;
        }

        void setFeatureForIph(@Nullable String feature) {
            mFeature = feature;
        }

        void maybeEmitEventForIph(@Nullable Tracker tracker) {
            if (tracker != null && mFeature != null) {
                KeyboardAccessoryIphUtils.emitFillingEvent(tracker, mFeature);
            }
        }

        @Nullable String getFeatureForIph() {
            return mFeature;
        }

        @Override
        public String toString() {
            return "Autofill" + super.toString();
        }

        @VisibleForTesting
        public static @Type int getBarItemType(AutofillSuggestion suggestion, Profile profile) {
            AutofillProfilePayload payload = suggestion.getAutofillProfilePayload();
            switch (FillingProductBridge.getFillingProductFromSuggestionType(
                    suggestion.getSuggestionType())) {
                case FillingProduct.ADDRESS:
                    {
                        if (payload != null) {
                            PersonalDataManager personalDataManager =
                                    PersonalDataManagerFactory.getForProfile(profile);
                            AutofillProfile autofillProfile =
                                    personalDataManager.getProfile(payload.getGuid());
                            if (autofillProfile != null) {
                                @RecordType int type = autofillProfile.getRecordType();
                                if (type == RecordType.ACCOUNT_HOME
                                        || type == RecordType.ACCOUNT_WORK) {
                                    return Type.HOME_AND_WORK_SUGGESTION;
                                }
                            }
                        }
                    }
                    break;
                case FillingProduct.CREDIT_CARD:
                case FillingProduct.IBAN:
                    return Type.PAYMENTS_SUGGESTION;
                case FillingProduct.LOYALTY_CARD:
                    return Type.LOYALTY_CARD_SUGGESTION;
                default:
                    return Type.SUGGESTION;
            }
            return Type.SUGGESTION;
        }
    }

    /**
     * A tab layout or a button group in a {@link RecyclerView} can be destroyed and recreated
     * whenever it is scrolled out of/into view. This wrapper allows to trigger a callback whenever
     * the view is recreated so it can be bound to its component.
     */
    static final class SheetOpenerBarItem extends ActionBarItem {
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

    /**
     * A {@link BarItem} that represents a "Dismiss" button.
     *
     * <p>This item triggers the provided runnable, which handles the logic for closing the
     * associated keyboard accessory.
     */
    static final class DismissBarItem extends ActionBarItem {
        DismissBarItem(Runnable dismissRunnable) {
            super(
                    Type.DISMISS_CHIP,
                    new Action(
                            AccessoryAction.DISMISS,
                            unused -> {
                                ManualFillingMetricsRecorder.recordActionSelected(
                                        AccessoryAction.DISMISS);
                                dismissRunnable.run();
                            }),
                    R.string.keyboard_accessory_dismiss_button);
        }
    }

    private KeyboardAccessoryProperties() {}
}
