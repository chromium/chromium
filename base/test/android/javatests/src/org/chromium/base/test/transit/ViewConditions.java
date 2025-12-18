// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW;
import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW_WITH_BOUNDS;

import android.view.View;

import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/** {@link Condition}s related to Android {@link View}s. */
@NullMarked
class ViewConditions {
    static String writeMatchingViewsStatusMessage(List<ViewAndRoot> viewMatches) {
        // TODO(crbug.com/456770151): Print which root matches are in.
        if (viewMatches.isEmpty()) {
            return "No matching Views";
        } else if (viewMatches.size() == 1) {
            String viewDescription =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            return String.format("1 matching View: %s", viewDescription);
        } else {
            String viewDescription1 =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            String viewDescription2 =
                    ViewPrinter.describeView(viewMatches.get(1).view, PRINT_SHALLOW);
            String moreString = viewMatches.size() > 2 ? " and more" : "";

            return String.format(
                    "%d matching Views: %s, %s%s",
                    viewMatches.size(), viewDescription1, viewDescription2, moreString);
        }
    }

    static String writeDisplayedViewsStatusMessage(List<DisplayedEvaluation> displayedEvaluations) {
        // TODO(crbug.com/456770151): Print which root matches are in.
        if (displayedEvaluations.isEmpty()) {
            return "No matching Views";
        } else if (displayedEvaluations.size() == 1) {
            String viewDescription = writeDisplayedViewStatus(displayedEvaluations.get(0));
            return String.format("1 matching View: %s", viewDescription);
        } else {
            String viewDescription1 = writeDisplayedViewStatus(displayedEvaluations.get(0));
            String viewDescription2 = writeDisplayedViewStatus(displayedEvaluations.get(1));
            String moreString = displayedEvaluations.size() > 2 ? " and more" : "";

            return String.format(
                    "%d matching Views: %s, %s%s",
                    displayedEvaluations.size(), viewDescription1, viewDescription2, moreString);
        }
    }

    private static String writeDisplayedViewStatus(DisplayedEvaluation displayedEvaluation) {
        List<String> messages = new ArrayList<>();
        messages.add(
                ViewPrinter.describeView(
                        displayedEvaluation.viewAndRoot.view, PRINT_SHALLOW_WITH_BOUNDS));
        messages.addAll(displayedEvaluation.messages);
        return String.join("; ", messages);
    }

    static DisplayedEvaluation evaluateMatch(
            ViewAndRoot viewAndRoot, int displayedPercentageRequired) {
        DisplayedEvaluation matchEvaluation = new DisplayedEvaluation(viewAndRoot);

        View matchedView = viewAndRoot.view;

        int visibility = matchedView.getVisibility();
        if (visibility != View.VISIBLE) {
            matchEvaluation.didMatch = false;
            matchEvaluation.messages.add(
                    String.format("visibility = %s", visibilityIntToString(visibility)));
        } else {
            View view = matchedView;
            while (view.getParent() instanceof View) {
                view = (View) view.getParent();
                visibility = view.getVisibility();
                if (visibility != View.VISIBLE) {
                    matchEvaluation.didMatch = false;
                    matchEvaluation.messages.add(
                            String.format(
                                    "visibility of ancestor [%s] = %s",
                                    ViewPrinter.describeView(view, PRINT_SHALLOW),
                                    visibilityIntToString(visibility)));
                    break;
                }
            }
        }

        if (displayedPercentageRequired > 0) {
            DisplayedPortion portion = DisplayedPortion.ofView(matchedView);
            if (portion.mPercentage < displayedPercentageRequired) {
                matchEvaluation.didMatch = false;
                matchEvaluation.messages.add(
                        String.format(
                                "%d%% displayed, expected >= %d%%",
                                portion.mPercentage, displayedPercentageRequired));
                matchEvaluation.messages.add("% displayed calculation: " + portion);
            } else {
                matchEvaluation.messages.add(String.format("%d%% displayed", portion.mPercentage));
            }
        }

        return matchEvaluation;
    }

    private static String visibilityIntToString(int visibility) {
        return switch (visibility) {
            case View.VISIBLE -> "VISIBLE";
            case View.INVISIBLE -> "INVISIBLE";
            case View.GONE -> "GONE";
            default -> "invalid";
        };
    }

    static class DisplayedEvaluation {
        public boolean didMatch = true;
        public final List<String> messages = new ArrayList<>();
        public ViewAndRoot viewAndRoot;

        DisplayedEvaluation(ViewAndRoot viewAndRoot) {
            this.viewAndRoot = viewAndRoot;
        }
    }
}
