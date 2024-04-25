// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.any;

import android.content.res.Resources;
import android.view.View;

import androidx.test.espresso.AmbiguousViewMatcherException;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import java.util.ArrayList;
import java.util.regex.Pattern;

/** {@link Condition}s related to Android {@link View}s. */
public class ViewConditions {
    /** Fulfilled when a single matching View exists and is displayed. */
    public static class DisplayedCondition extends ExistsCondition {
        public DisplayedCondition(Matcher<View> matcher, ExistsCondition.Options options) {
            super(allOf(matcher, isDisplayed()), options);
        }
    }

    /**
     * Fulfilled when a single matching View exists and is displayed, but ignored if |gate| returns
     * true.
     */
    public static class GatedDisplayedCondition extends InstrumentationThreadCondition {

        private final DisplayedCondition mDisplayedCondition;
        private final Condition mGate;

        public GatedDisplayedCondition(
                Matcher<View> matcher, Condition gate, ExistsCondition.Options options) {
            super();
            mDisplayedCondition = new DisplayedCondition(matcher, options);
            mGate = gate;
        }

        @Override
        public ConditionStatus check() throws Exception {
            ConditionStatus gateStatus = mGate.check();
            String gateMessage = gateStatus.getMessageAsGate();
            if (!gateStatus.isFulfilled()) {
                return fulfilled(gateMessage);
            }

            ConditionStatus status = mDisplayedCondition.check();
            status.amendMessage(gateMessage);
            return status;
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "%s (if %s)", mDisplayedCondition.buildDescription(), mGate.buildDescription());
        }
    }

    /** Fulfilled when a single matching View exists. */
    public static class ExistsCondition extends InstrumentationThreadCondition {
        private final Matcher<View> mMatcher;
        private final Options mOptions;
        private View mViewMatched;

        public ExistsCondition(Matcher<View> matcher, Options options) {
            super();
            mMatcher = matcher;
            mOptions = options;
        }

        @Override
        public String buildDescription() {
            return "View: " + ViewConditions.createMatcherDescription(mMatcher);
        }

        @Override
        public ConditionStatus check() {
            ViewInteraction viewInteraction = onView(mMatcher);
            String[] message = new String[1];
            try {
                viewInteraction.perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return any(View.class);
                            }

                            @Override
                            public String getDescription() {
                                return "check exists and consistent";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                if (mViewMatched != null && mViewMatched != view) {
                                    message[0] =
                                            String.format(
                                                    "Matched a different view, was %s, now %s",
                                                    mViewMatched, view);
                                }
                                mViewMatched = view;
                            }
                        });
                if (mOptions.mExpectEnabled) {
                    if (!mViewMatched.isEnabled()) {
                        return notFulfilled("View displayed but disabled");
                    }
                } else { // Expected a displayed but disabled View.
                    if (mViewMatched.isEnabled()) {
                        return notFulfilled("View displayed but enabled");
                    }
                }
                return fulfilled(message[0]);
            } catch (NoMatchingViewException
                    | NoMatchingRootException
                    | AmbiguousViewMatcherException e) {
                if (mViewMatched != null) {
                    throw new IllegalStateException(
                            String.format(
                                    "Had matched a view (%s), but now got %s",
                                    mViewMatched, e.getClass().getSimpleName()),
                            e);
                }
                return notFulfilled(e.getClass().getSimpleName());
            }
        }

        /**
         * @return an Options builder to customize the ViewCondition.
         */
        public static Options.Builder newOptions() {
            return new Options().new Builder();
        }

        /** Extra options for declaring ExistsCondition. */
        public static class Options {
            boolean mExpectEnabled = true;

            private Options() {}

            public class Builder {
                public Options build() {
                    return Options.this;
                }

                /** Whether the view is expected to be enabled or disabled. */
                public Builder withExpectEnabled(boolean state) {
                    mExpectEnabled = state;
                    return this;
                }
            }
        }
    }

    /** Fulfilled when no matching Views exist and are displayed. */
    public static class NotDisplayedAnymoreCondition extends InstrumentationThreadCondition {
        private final Matcher<View> mMatcher;

        public NotDisplayedAnymoreCondition(Matcher<View> matcher) {
            super();
            mMatcher = allOf(matcher, isDisplayed());
        }

        @Override
        public String buildDescription() {
            return "No more view: " + ViewConditions.createMatcherDescription(mMatcher);
        }

        @Override
        public ConditionStatus check() {
            try {
                onView(mMatcher).check(doesNotExist());
                return fulfilled();
            } catch (AssertionError e) {
                return notFulfilled();
            }
        }
    }

    private static String getResourceName(int resId) {
        return InstrumentationRegistry.getInstrumentation()
                .getContext()
                .getResources()
                .getResourceName(resId);
    }

    /** Generates a description for the matcher that replaces raw ids with resource names. */
    private static String createMatcherDescription(Matcher<View> matcher) {
        StringDescription d = new StringDescription();
        matcher.describeTo(d);
        String description = d.toString();
        Pattern numberPattern = Pattern.compile("[0-9]+");
        java.util.regex.Matcher numberMatcher = numberPattern.matcher(description);
        ArrayList<Integer> starts = new ArrayList<>();
        ArrayList<Integer> ends = new ArrayList<>();
        ArrayList<String> resourceNames = new ArrayList<>();
        while (numberMatcher.find()) {
            int resourceId = Integer.parseInt(numberMatcher.group());
            if (resourceId > 0xFFFFFF) {
                // Build-time Android resources have ids > 0xFFFFFF
                starts.add(numberMatcher.start());
                ends.add(numberMatcher.end());
                String resourceDescription = createResourceDescription(resourceId);
                resourceNames.add(resourceDescription);
            } else {
                resourceNames.add(numberMatcher.group());
            }
        }

        if (starts.size() == 0) return description;

        String newDescription = description.substring(0, starts.get(0));
        for (int i = 0; i < starts.size(); i++) {
            newDescription += resourceNames.get(i);
            int nextStart = (i == starts.size() - 1) ? description.length() : starts.get(i + 1);
            newDescription += description.substring(ends.get(i), nextStart);
        }

        return newDescription;
    }

    private static String createResourceDescription(int possibleResourceId) {
        try {
            return getResourceName(possibleResourceId);
        } catch (Resources.NotFoundException e) {
            return String.valueOf(possibleResourceId);
        }
    }
}
