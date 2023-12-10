// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
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
    /** The Recorder itself should be stateless and have no need for an instance. */
    private KeyboardAccessoryMetricsRecorder() {}

    /**
     * This observer will react to changes of the {@link KeyboardAccessoryProperties} and store each
     * impression once per visibility change.
     */
    private static class AccessoryBarObserver
            implements ListObservable.ListObserver<Void>,
                    PropertyObservable.PropertyObserver<PropertyKey> {
        private final Set<Integer> mRecordedActionImpressions = new HashSet<>();
        private final PropertyModel mModel;

        AccessoryBarObserver(PropertyModel keyboardAccessoryModel) {
            mModel = keyboardAccessoryModel;
        }

        @Override
        public void onPropertyChanged(
                PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
            if (propertyKey == VISIBLE) {
                if (!mModel.get(VISIBLE)) {
                    mRecordedActionImpressions.clear();
                }
                return;
            }
            if (propertyKey == KeyboardAccessoryProperties.BOTTOM_OFFSET_PX
                    || propertyKey == KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION
                    || propertyKey == KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING
                    || propertyKey == KeyboardAccessoryProperties.SHOW_SWIPING_IPH
                    || propertyKey == KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK
                    || propertyKey == KeyboardAccessoryProperties.HAS_SUGGESTIONS
                    || propertyKey == KeyboardAccessoryProperties.ANIMATION_LISTENER) {
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
                if (mRecordedActionImpressions.add(action.getActionType())) {
                    ManualFillingMetricsRecorder.recordActionImpression(action.getActionType());
                }
            }
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
    }

    /**
     * Registers an observer to the given model that records changes for all properties.
     * @param keyboardAccessoryModel The observable {@link KeyboardAccessoryProperties}.
     */
    static void registerKeyboardAccessoryModelMetricsObserver(
            PropertyModel keyboardAccessoryModel) {
        AccessoryBarObserver observer = new AccessoryBarObserver(keyboardAccessoryModel);
        keyboardAccessoryModel.addObserver(observer);
        keyboardAccessoryModel.get(BAR_ITEMS).addObserver(observer);
    }
}
