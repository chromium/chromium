// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

/** Spot checks multiple {@link Condition}s to assert preconditions are still valid. */
public class ConditionChecker {

    /** The fulfillment status of a {@link Condition} being checked once. */
    private static class ConditionCheckStatus {

        private final Condition mCondition;
        private boolean mFulfilled;
        private String mError;

        private ConditionCheckStatus(Condition condition) {
            mCondition = condition;
        }

        private boolean update() {
            try {
                boolean fulfilled;
                if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/1489445): Post multiple checks in parallel, the UI thread will
                    // run them sequentially.
                    fulfilled = ThreadUtils.runOnUiThreadBlocking(mCondition::check);
                } else {
                    fulfilled = mCondition.check();
                }

                if (fulfilled) {
                    reportFulfilled();
                    return false;
                } else {
                    reportUnfulfilled();
                    return true;
                }
            } catch (Exception e) {
                reportError(e);
                return true;
            }
        }

        private void reportFulfilled() {
            mFulfilled = true;
        }

        private void reportUnfulfilled() {
            mFulfilled = false;
        }

        private void reportError(Exception e) {
            mError = e.getMessage();
        }

        private boolean isFulfilled() {
            return mFulfilled;
        }

        private String getError() {
            return mError;
        }
    }

    /**
     * Spot checks each of the {@link Condition}s.
     *
     * @param conditions the {@link Condition}s to check.
     * @throws AssertionError if not all Conditions are fulfilled.
     */
    public static void check(List<Condition> conditions) {
        boolean anyCriteriaMissing = false;
        List<ConditionCheckStatus> checkStatuses = new ArrayList<>();
        for (Condition condition : conditions) {
            checkStatuses.add(new ConditionCheckStatus(condition));
        }

        for (ConditionCheckStatus status : checkStatuses) {
            anyCriteriaMissing |= status.update();
        }

        if (anyCriteriaMissing) {
            throw buildCheckConditionsException(checkStatuses);
        }
    }

    private static AssertionError buildCheckConditionsException(
            List<ConditionCheckStatus> checkStatuses) {
        return new AssertionError(
                "Preconditions not fulfilled:\n" + createCheckConditionsSummary(checkStatuses));
    }

    private static String createCheckConditionsSummary(List<ConditionCheckStatus> checkStatuses) {
        StringBuilder detailsString = new StringBuilder();

        int i = 1;
        for (ConditionCheckStatus checkStatus : checkStatuses) {
            String conditionDescription = checkStatus.mCondition.getDescription();

            String error = checkStatus.getError();
            String errorsString = null;
            String statusString;
            if (error != null) {
                errorsString = String.format(" {error: %s}", error);
                statusString = "[ERR ]";
            } else {
                if (checkStatus.isFulfilled()) {
                    statusString = "[OK  ]";
                } else {
                    statusString = "[FAIL]";
                }
            }

            detailsString
                    .append("    [")
                    .append(i)
                    .append(" ")
                    .append(statusString)
                    .append(" ")
                    .append(conditionDescription);
            if (errorsString != null) {
                detailsString.append(" ").append(errorsString);
            }
            detailsString.append('\n');
            i++;
        }
        return detailsString.toString();
    }
}
