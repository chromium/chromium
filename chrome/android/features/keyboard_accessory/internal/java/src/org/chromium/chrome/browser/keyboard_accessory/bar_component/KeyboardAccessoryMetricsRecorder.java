// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryBarContents;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.HashSet;
import java.util.Set;

/**
 * This class provides helpers to record metrics related to the keyboard accessory bar.
 * It sets up an observer to observe {@link KeyboardAccessoryProperties}-based models and records
 * metrics accordingly.
 */
class KeyboardAccessoryMetricsRecorder {
    static final String UMA_KEYBOARD_ACCESSORY_BAR_SHOWN = "KeyboardAccessory.AccessoryBarShown";

    /**
     * The Recorder itself should be stateless and have no need for an instance.
     */
    private KeyboardAccessoryMetricsRecorder() {}

    /**
     * This observer will react to changes of the {@link KeyboardAccessoryProperties} and store each
     * impression once per visibility change.
     */
    private static class AccessoryBarObserver
            implements ListObservable.ListObserver<Void>,
                       PropertyObservable.PropertyObserver<PropertyKey> {
        private final Set<Integer> mRecordedBarBuckets = new HashSet<>();
        private final Set<Integer> mRecordedActionImpressions = new HashSet<>();
        private final PropertyModel mModel;
        private final KeyboardAccessoryCoordinator.TabSwitchingDelegate mTabSwitcher;

        AccessoryBarObserver(PropertyModel keyboardAccessoryModel,
                KeyboardAccessoryCoordinator.TabSwitchingDelegate tabSwitcher) {
            mModel = keyboardAccessoryModel;
            mTabSwitcher = tabSwitcher;
        }

        @Override
        public void onPropertyChanged(
                PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
            if (propertyKey == VISIBLE) {
                if (mModel.get(VISIBLE)) {
                    recordFirstImpression();
                    maybeRecordBarBucket(AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS);
                    maybeRecordBarBucket(AccessoryBarContents.WITH_TABS);
                    recordGeneralActionTypes();
                } else {
                    mRecordedBarBuckets.clear();
                    mRecordedActionImpressions.clear();
                }
                return;
            }
            if (propertyKey == KeyboardAccessoryProperties.BOTTOM_OFFSET_PX
                    || propertyKey == KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE
                    || propertyKey == KeyboardAccessoryProperties.SHEET_TITLE
                    || propertyKey == KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK
                    || propertyKey == KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION
                    || propertyKey == KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING) {
                return;
            }
            assert false : "Every property update needs to be handled explicitly!";
        }

        /**
         * If not done yet, this records an impression for the general type of list that was added.
         * In addition, it records impressions for each new action type that changed in the list.
         * @param l A list of {@link BarItem}s. Must be equal to the observed models list.
         * @param first Index of the first element that changed.
         * @param count Number of elements starting with |first| that were added or changed.
         */
        private void recordUnrecordedList(ListObservable l, int first, int count) {
            assert l == mModel.get(BAR_ITEMS) : "Tried to record metrics for unknown list " + l;
            // Record any unrecorded type, but not more than once (i.e. one set of suggestion).
            for (int index = first; index < first + count; ++index) {
                KeyboardAccessoryData.Action action = mModel.get(BAR_ITEMS).get(index).getAction();
                if (action == null) continue; // Item is no relevant action.
                maybeRecordBarBucket(action.getActionType() == AccessoryAction.AUTOFILL_SUGGESTION
                                ? AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS
                                : AccessoryBarContents.WITH_ACTIONS);
                if (mRecordedActionImpressions.add(action.getActionType())) {
                    ManualFillingMetricsRecorder.recordActionImpression(action.getActionType());
                }
            }
        }

        private void recordGeneralActionTypes() {
            if (!mModel.get(VISIBLE)) return;
            // Record any unrecorded type, but not more than once (i.e. one set of suggestion).
            for (int index = 0; index < mModel.get(BAR_ITEMS).size(); ++index) {
                KeyboardAccessoryData.Action action = mModel.get(BAR_ITEMS).get(index).getAction();
                if (action == null) continue; // Item is no relevant action.
                maybeRecordBarBucket(action.getActionType() == AccessoryAction.AUTOFILL_SUGGESTION
                                ? AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS
                                : AccessoryBarContents.WITH_ACTIONS);
            }
        }

        /**
         * Records whether the first impression of the bar contained any contents (which it should).
         */
        private void recordFirstImpression() {
            if (!mRecordedBarBuckets.isEmpty()) return;
            @AccessoryBarContents
            int bucketToRecord = AccessoryBarContents.NO_CONTENTS;
            for (@AccessoryBarContents int bucket = 0; bucket < AccessoryBarContents.COUNT;
                    ++bucket) {
                if (shouldRecordAccessoryBarImpression(bucket)) {
                    bucketToRecord = AccessoryBarContents.ANY_CONTENTS;
                    break;
                }
            }
            maybeRecordBarBucket(bucketToRecord);
        }

        @Override
        public void onItemRangeInserted(ListObservable source, int index, int count) {
            recordUnrecordedList(source, index, count);
        }

        @Override
        public void onItemRangeRemoved(ListObservable source, int index, int count) {}

        @Override
        public void onItemRangeChanged(
                ListObservable<Void> source, int index, int count, @Nullable Void payload) {
            // Remove all actions that were changed, so changes are treated as new recordings.
            for (int i = index; i < index + count; ++i) {
                KeyboardAccessoryData.Action action = mModel.get(BAR_ITEMS).get(i).getAction();
                if (action == null) continue; // Item is no recordable action.
                mRecordedActionImpressions.remove(action.getActionType());
            }
            recordUnrecordedList(source, index, count);
        }

        /**
         * Returns an impression for the accessory bar if it hasn't occurred yet.
         * @param bucket The bucket to record.
         */
        private void maybeRecordBarBucket(@AccessoryBarContents int bucket) {
            if (!shouldRecordAccessoryBarImpression(bucket)) return;
            mRecordedBarBuckets.add(bucket);
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_KEYBOARD_ACCESSORY_BAR_SHOWN, bucket, AccessoryBarContents.COUNT);
        }

        /**
         * If a checks whether the given bucket should be recorded (i.e. the property it observes is
         * not empty, the accessory is visible and it wasn't recorded yet).
         * @param bucket
         * @return
         */
        private boolean shouldRecordAccessoryBarImpression(int bucket) {
            if (!mModel.get(VISIBLE)) return false;
            if (mRecordedBarBuckets.contains(bucket)) return false;
            switch (bucket) {
                case AccessoryBarContents.WITH_ACTIONS:
                    return hasAtLeastOneActionOfType(mModel.get(BAR_ITEMS),
                            AccessoryAction.MANAGE_PASSWORDS,
                            AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
                case AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS:
                    return hasAtLeastOneActionOfType(
                            mModel.get(BAR_ITEMS), AccessoryAction.AUTOFILL_SUGGESTION);
                case AccessoryBarContents.WITH_TABS:
                    return mTabSwitcher.hasTabs();
                case AccessoryBarContents.ANY_CONTENTS: // Intentional fallthrough.
                case AccessoryBarContents.NO_CONTENTS:
                    return true; // Logged on first impression.
            }
            assert false : "Did not check whether to record an impression bucket " + bucket + ".";
            return false;
        }
    }

    /**
     * Registers an observer to the given model that records changes for all properties.
     * @param keyboardAccessoryModel The observable {@link KeyboardAccessoryProperties}.
     */
    static void registerKeyboardAccessoryModelMetricsObserver(PropertyModel keyboardAccessoryModel,
            KeyboardAccessoryCoordinator.TabSwitchingDelegate tabSwitcher) {
        AccessoryBarObserver observer =
                new AccessoryBarObserver(keyboardAccessoryModel, tabSwitcher);
        keyboardAccessoryModel.addObserver(observer);
        keyboardAccessoryModel.get(BAR_ITEMS).addObserver(observer);
    }

    private static boolean hasAtLeastOneActionOfType(
            ListModel<BarItem> itemList, @AccessoryAction int... types) {
        Set<Integer> typeList = new HashSet<>(types.length);
        for (@AccessoryAction int type : types) typeList.add(type);
        for (BarItem barItem : itemList) {
            if (barItem.getAction() == null) continue; // Item irrelevant for recording.
            if (typeList.contains(barItem.getAction().getActionType())) return true;
        }
        return false;
    }
}
