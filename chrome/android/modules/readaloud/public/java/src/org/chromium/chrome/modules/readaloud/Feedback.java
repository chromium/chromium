// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.build.annotations.NullMarked;
import java.util.Locale;

/** Enum definitions for Audio Overviews feedback args. */
@NullMarked
public class Feedback {
    public enum FeedbackType {
        NONE(0),
        POSITIVE(1),
        NEGATIVE(2);

        private final int mValue;

        FeedbackType(int value) {
            mValue = value;
        }

        public int getValue() {
            return mValue;
        }

        public static FeedbackType fromValue(int value) {
            for (FeedbackType type : values()) {
                if (type.getValue() == value) {
                    return type;
                }
            }
            throw new IllegalArgumentException("Unknown value: " + value);
        }
    }

    public enum NegativeFeedbackReason {
        NOT_FACTUALLY_CORRECT(0),
        BAD_VOICE(1),
        NOT_ENGAGING(2),
        OFFENSIVE(3),
        TECHNICAL_ISSUE(4),
        OTHER(5);

        private final int mValue;

        NegativeFeedbackReason(int value) {
            mValue = value;
        }

        public int getValue() {
            return mValue;
        }

        public static NegativeFeedbackReason fromValue(int value) {
            for (NegativeFeedbackReason reason : values()) {
                if (reason.getValue() == value) {
                    return reason;
                }
            }
            throw new IllegalArgumentException("Unknown value: " + value);
        }

        @Override
        public String toString() {
          return String.format(Locale.US, "%s (%d)", this.name(), this.getValue());
        }
    }
}