// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/** An input in a form. */
@JNINamespace("autofill_assistant")
public abstract class AssistantFormInput {
    /** Create a view associated to this input. */
    public abstract View createView(Context context, ViewGroup parent);

    // TODO(crbug.com/806868): Check if it's possible to create generic methods createList, add, etc
    // to manipulate java lists from native code, or reuse if they already exist.
    @CalledByNative
    private static List<AssistantFormCounter> createCounterList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static List<AssistantFormSelectionChoice> createChoiceList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addCounter(
            List<AssistantFormCounter> counters, AssistantFormCounter counter) {
        counters.add(counter);
    }

    @CalledByNative
    private static void addChoice(
            List<AssistantFormSelectionChoice> choices, AssistantFormSelectionChoice choice) {
        choices.add(choice);
    }

    @CalledByNative
    private static AssistantFormCounter createCounter(String label, String subtext,
            int initialValue, int minValue, int maxValue, int[] allowedValues) {
        return AssistantFormCounter.create(
                label, subtext, initialValue, minValue, maxValue, allowedValues);
    }

    @CalledByNative
    private static AssistantFormSelectionChoice createChoice(
            String label, boolean initiallySelected) {
        return new AssistantFormSelectionChoice(label, initiallySelected);
    }

    @CalledByNative
    private static AssistantFormCounterInput createCounterInput(int inputIndex, String label,
            String expandText, String minimizeText, List<AssistantFormCounter> counters,
            int minimizedCount, long minCountersSum, long maxCountersSum,
            AssistantFormDelegate delegate) {
        return new AssistantFormCounterInput(label, expandText, minimizeText, counters,
                minimizedCount, minCountersSum, maxCountersSum,
                (counterIndex,
                        value) -> delegate.onCounterChanged(inputIndex, counterIndex, value));
    }

    @CalledByNative
    private static AssistantFormSelectionInput createSelectionInput(int inputIndex, String label,
            List<AssistantFormSelectionChoice> choices, boolean allowMultipleChoices,
            AssistantFormDelegate delegate) {
        return new AssistantFormSelectionInput(label, choices, allowMultipleChoices,
                (choiceIndex, selected)
                        -> delegate.onChoiceSelectionChanged(inputIndex, choiceIndex, selected));
    }
}
