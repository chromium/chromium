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
            ViewAndRoot viewAndRoot,
            int displayedPercentageRequired,
            int expectedEffectiveVisibility) {
        DisplayedEvaluation matchEvaluation = new DisplayedEvaluation(viewAndRoot);

        View matchedView = viewAndRoot.view;

        if (!matchedView.isAttachedToWindow()) {
            matchEvaluation.didMatch = false;
            matchEvaluation.messages.add("Detached from window");
        }

        // Calculate the actual effective visibility considering ancestors.
        //
        // Traverses the entire hierarchy to the root to find the highest ancestor determining
        // visibility. This provides a more stable "cause" for failures than stopping at the first
        // non-visible ancestor.
        //
        // Priority:
        // 1. GONE (Takes no space. Any ancestor being GONE makes the View effectively GONE).
        // 2. INVISIBLE (Takes space. Any ancestor being INVISIBLE makes the View effectively
        //    INVISIBLE, unless a higher ancestor is GONE).
        // 3. VISIBLE (Takes space and is drawn).
        int actualEffectiveVisibility = matchedView.getVisibility();
        View ancestorDeterminingVisibility = null;
        View view = matchedView;
        while (view.getParent() instanceof View) {
            view = (View) view.getParent();
            int visibility = view.getVisibility();
            if (visibility == View.GONE) {
                actualEffectiveVisibility = View.GONE;
                ancestorDeterminingVisibility = view;
            } else if (actualEffectiveVisibility != View.GONE && visibility == View.INVISIBLE) {
                actualEffectiveVisibility = View.INVISIBLE;
                ancestorDeterminingVisibility = view;
            }
        }

        // Check effective visibility and report cause.
        if (actualEffectiveVisibility != expectedEffectiveVisibility) {
            matchEvaluation.didMatch = false;
            matchEvaluation.messages.add(
                    String.format(
                            "effectively %s", visibilityIntToString(actualEffectiveVisibility)));
            if (ancestorDeterminingVisibility != null) {
                matchEvaluation.messages.add(
                        String.format(
                                "due to ancestor %s",
                                ViewPrinter.describeView(
                                        ancestorDeterminingVisibility, PRINT_SHALLOW)));
            }
        }

        // Calculate, check and report displayed percentage.
        if (displayedPercentageRequired > 0) {
            DisplayedPortion portion = DisplayedPortion.ofView(matchedView);
            boolean shouldOccupySpace = expectedEffectiveVisibility != View.GONE;

            // GONE views take no space, so displayed percentage is irrelevant (and likely 0).
            // INVISIBLE views still occupy layout space and have valid screen bounds (via
            // getGlobalVisibleRect()), so we verify they are correctly positioned on-screen.
            if (shouldOccupySpace && portion.mPercentage < displayedPercentageRequired) {
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

    static String visibilityIntToString(int visibility) {
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
