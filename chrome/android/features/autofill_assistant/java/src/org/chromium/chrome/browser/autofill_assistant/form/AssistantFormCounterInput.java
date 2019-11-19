// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.util.AccessibilityUtil;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

/** A form input that allows to modify one or multiple counters. */
class AssistantFormCounterInput extends AssistantFormInput {
    private static final String QUOTED_VALUE = Pattern.quote("{value}");
    private static final Transition EXPAND_TRANSITION =
            new TransitionSet()
                    .setOrdering(TransitionSet.ORDERING_TOGETHER)
                    .addTransition(new Fade(Fade.OUT))
                    .addTransition(new ChangeBounds())
                    .addTransition(new Fade(Fade.IN));

    interface Delegate {
        void onCounterChanged(int counterIndex, int value);
    }

    private static class CounterViewHolder {
        private final View mView;
        private final TextView mLabelView;
        private final TextView mSubtextView;
        private final TextView mValueView;
        private final View mDecreaseButtonView;
        private final View mIncreaseButtonView;

        private CounterViewHolder(Context context) {
            mView = LayoutInflater.from(context).inflate(
                    R.layout.autofill_assistant_form_counter, /*root= */ null);
            mLabelView = mView.findViewById(R.id.label);
            mSubtextView = mView.findViewById(R.id.subtext);
            mValueView = mView.findViewById(R.id.value);
            mDecreaseButtonView = mView.findViewById(R.id.decrease_button);
            mIncreaseButtonView = mView.findViewById(R.id.increase_button);
        }
    }

    private final String mLabel;
    private final String mExpandText;
    private final String mMinimizeText;
    private final List<AssistantFormCounter> mCounters;

    /**
     * The (minimum) number of counters to show when this input is minimized. The counters with
     * count > 0 will always be shown, so the number of counters actually shown when this input is
     * minimized might be strictly larger than |mMinimizedCount|.
     */
    private final int mMinimizedCount;
    private final long mMinCountersSum;
    private final long mMaxCountersSum;
    private final Delegate mDelegate;

    AssistantFormCounterInput(String label, String expandText, String minimizeText,
            List<AssistantFormCounter> counters, int minimizedCount, long minCountersSum,
            long maxCountersSum, Delegate delegate) {
        mLabel = label;
        mExpandText = expandText;
        mMinimizeText = minimizeText;
        mCounters = counters;

        // Don't show the expandable section if there is no text to show when minimized/expanded, or
        // when TalkBack is enabled.
        mMinimizedCount = expandText.isEmpty() || minimizeText.isEmpty()
                        || AccessibilityUtil.isAccessibilityEnabled()
                ? Integer.MAX_VALUE
                : minimizedCount;
        mMinCountersSum = minCountersSum;
        mMaxCountersSum = maxCountersSum;
        mDelegate = delegate;
    }

    @Override
    public View createView(Context context, ViewGroup parent) {
        ViewGroup root = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_form_counter_input, parent, /* attachToRoot= */ false);
        TextView label = root.findViewById(R.id.label);
        if (mLabel.isEmpty()) {
            label.setVisibility(View.GONE);
        } else {
            label.setText(mLabel);
        }

        // Create the views.
        int labelIndex = root.indexOfChild(label);
        List<CounterViewHolder> viewHolders = new ArrayList<>();
        for (int i = 0; i < mCounters.size(); i++) {
            CounterViewHolder viewHolder = new CounterViewHolder(context);
            viewHolders.add(viewHolder);

            // Add the counters below the label.
            root.addView(viewHolder.mView, labelIndex + i + 1);
        }

        // Initialize the views and attach listeners.
        initializeCounterViews(mCounters, viewHolders);

        // If some counters are hidden in the minimized state, show the expand label that will show
        // them once clicked.
        if (mCounters.size() > mMinimizedCount) {
            setViewsVisibility(mCounters, viewHolders, /* minimized= */ true);

            ViewGroup expandLabelContainer = root.findViewById(R.id.expand_label_container);
            TextView expandLabel = expandLabelContainer.findViewById(R.id.expand_label);
            TextView minimizeLabel = expandLabelContainer.findViewById(R.id.minimize_label);
            View chevron = expandLabelContainer.findViewById(R.id.chevron);

            expandLabel.setText(mExpandText);
            minimizeLabel.setText(mMinimizeText);
            minimizeLabel.setVisibility(View.GONE);
            expandLabelContainer.setVisibility(View.VISIBLE);

            expandLabelContainer.setOnClickListener(unusedView -> {
                TransitionManager.beginDelayedTransition(
                        (ViewGroup) root.getRootView(), EXPAND_TRANSITION);
                boolean shouldMinimize = expandLabel.getVisibility() == View.GONE;
                if (shouldMinimize) {
                    expandLabel.setVisibility(View.VISIBLE);
                    minimizeLabel.setVisibility(View.GONE);
                    chevron.animate().rotation(0).start();
                } else {
                    expandLabel.setVisibility(View.GONE);
                    minimizeLabel.setVisibility(View.VISIBLE);
                    chevron.animate().rotation(180).start();
                }

                setViewsVisibility(mCounters, viewHolders, shouldMinimize);
            });
        }

        return root;
    }

    private void setViewsVisibility(List<AssistantFormCounter> counters,
            List<CounterViewHolder> viewHolders, boolean minimized) {
        if (!minimized) {
            for (CounterViewHolder viewHolder : viewHolders) {
                viewHolder.mView.setVisibility(View.VISIBLE);
            }
            return;
        }

        // Count the number of counters with count > 0.
        // TODO(crbug.com/806868): The magic value 0 makes sense for tickets, but might not make
        // sense for other type of counters. When using this counter input for other things than
        // tickets, we might want to disable this logic and just show the first mMinimizedCount
        // counters.
        int nonZeroCounters = 0;
        for (int i = 0; i < counters.size(); i++) {
            if (counters.get(i).getValue() > 0) {
                nonZeroCounters++;
            }
        }

        // Set the views visibility such that:
        //  - all counters with value > 0 are visible.
        //  - the first (mMinimizedCount - nonZeroCounters) counters with value = 0 are shown.
        //  - the remaining counters are hidden.
        int zeroCountersShown = mMinimizedCount - nonZeroCounters;
        for (int i = 0; i < counters.size(); i++) {
            viewHolders.get(i).mView.setVisibility(
                    counters.get(i).getValue() > 0 || zeroCountersShown-- > 0 ? View.VISIBLE
                                                                              : View.GONE);
        }
    }

    private void initializeCounterViews(
            List<AssistantFormCounter> counters, List<CounterViewHolder> views) {
        assert counters.size() == views.size();

        for (int i = 0; i < counters.size(); i++) {
            AssistantFormCounter counter = counters.get(i);
            CounterViewHolder view = views.get(i);

            if (!counter.getSubtext().isEmpty()) {
                view.mSubtextView.setVisibility(View.VISIBLE);
                view.mSubtextView.setText(counter.getSubtext());
            }

            updateLabelAndValue(counter, view);

            int index = i; // required for lambda.
            view.mDecreaseButtonView.setOnClickListener(unusedView
                    -> updateCounter(counters, views, index, /* increaseValue= */ false));
            view.mIncreaseButtonView.setOnClickListener(
                    unusedView -> updateCounter(counters, views, index, /* increaseValue= */ true));
        }

        updateButtonsState(counters, views);
    }

    // It is ok to suppress the warning here as we are only setting a number value to the TextView.
    @SuppressWarnings("SetTextI18n")
    private void updateLabelAndValue(AssistantFormCounter counter, CounterViewHolder view) {
        // Update the label.
        String label = counter.getLabel();
        label = label.replaceAll(QUOTED_VALUE, Integer.toString(counter.getValue()));
        view.mLabelView.setText(label);

        // Update the value view.
        view.mValueView.setText(Integer.toString(counter.getValue()));
    }

    private void updateButtonsState(
            List<AssistantFormCounter> counters, List<CounterViewHolder> views) {
        long sum = 0;
        for (AssistantFormCounter counter : counters) {
            sum += counter.getValue();
        }

        boolean canDecreaseSum = sum > mMinCountersSum;
        boolean canIncreaseSum = sum < mMaxCountersSum;

        for (int i = 0; i < counters.size(); i++) {
            AssistantFormCounter counter = counters.get(i);
            CounterViewHolder view = views.get(i);
            view.mDecreaseButtonView.setEnabled(canDecreaseSum && counter.canDecreaseValue());
            view.mIncreaseButtonView.setEnabled(canIncreaseSum && counter.canIncreaseValue());
        }
    }

    private void updateCounter(List<AssistantFormCounter> counters, List<CounterViewHolder> views,
            int counterIndex, boolean increaseValue) {
        AssistantFormCounter counter = counters.get(counterIndex);

        // Change the value.
        if (increaseValue) {
            counter.increaseValue();
        } else {
            counter.decreaseValue();
        }

        updateLabelAndValue(counter, views.get(counterIndex));
        updateButtonsState(counters, views);

        mDelegate.onCounterChanged(counterIndex, counter.getValue());
    }
}
