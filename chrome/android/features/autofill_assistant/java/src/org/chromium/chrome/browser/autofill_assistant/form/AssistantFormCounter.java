// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

abstract class AssistantFormCounter {
    private final String mLabel;
    private final String mSubtext;

    private AssistantFormCounter(String label, String subtext) {
        mLabel = label;
        mSubtext = subtext;
    }

    String getLabel() {
        return mLabel;
    }

    String getSubtext() {
        return mSubtext;
    }

    abstract int getValue();

    abstract boolean canDecreaseValue();

    abstract boolean canIncreaseValue();

    abstract void decreaseValue();

    abstract void increaseValue();

    static AssistantFormCounter create(String label, String subtext, int initialValue, int minValue,
            int maxValue, int[] allowedValues) {
        if (allowedValues.length > 0) {
            return new FiniteCounter(label, subtext, initialValue, allowedValues);
        }

        return new BoundedCounter(label, subtext, initialValue, minValue, maxValue);
    }

    /** A counter whose value is limited by a min and max value. */
    private static class BoundedCounter extends AssistantFormCounter {
        private final int mMinValue;
        private final int mMaxValue;
        private int mValue;

        private BoundedCounter(
                String label, String subtext, int initialValue, int minValue, int maxValue) {
            super(label, subtext);
            mMinValue = minValue;
            mMaxValue = maxValue;
            mValue = initialValue;
        }

        @Override
        int getValue() {
            return mValue;
        }

        @Override
        boolean canDecreaseValue() {
            return mValue > mMinValue;
        }

        @Override
        boolean canIncreaseValue() {
            return mValue < mMaxValue;
        }

        @Override
        void decreaseValue() {
            mValue = Math.max(mMinValue, mValue - 1);
        }

        @Override
        void increaseValue() {
            mValue = Math.min(mMaxValue, mValue + 1);
        }
    }

    /** A counter whose value is limited to a finite set of values. */
    private static class FiniteCounter extends AssistantFormCounter {
        private final int[] mAllowedValues;
        private int mValueIndex;

        private FiniteCounter(String label, String subtext, int initialValue, int[] allowedValues) {
            super(label, subtext);
            mAllowedValues = allowedValues;

            for (int i = 0; i < mAllowedValues.length; i++) {
                if (mAllowedValues[i] == initialValue) {
                    mValueIndex = i;
                    break;
                }
            }
        }

        @Override
        int getValue() {
            return mAllowedValues[mValueIndex];
        }

        @Override
        boolean canDecreaseValue() {
            return mValueIndex > 0;
        }

        @Override
        boolean canIncreaseValue() {
            return mValueIndex < mAllowedValues.length - 1;
        }

        @Override
        void decreaseValue() {
            mValueIndex = Math.max(0, mValueIndex - 1);
        }

        @Override
        void increaseValue() {
            mValueIndex = Math.min(mAllowedValues.length - 1, mValueIndex + 1);
        }
    }
}
