// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ConditionStatus.Status;

import java.util.ArrayList;
import java.util.List;

/** Spot checks multiple {@link Condition}s to assert preconditions are still valid. */
public class ConditionChecker {

    /** The process of checking a {@link Condition} only once. */
    private static class ConditionCheck {

        private final Condition mCondition;
        private ConditionStatus mStatus;

        private ConditionCheck(Condition condition) {
            mCondition = condition;
        }

        private boolean update() {
            try {
                if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/40284026): Post multiple checks in parallel, the UI thread
                    // will
                    // run them sequentially.
                    mStatus = ThreadUtils.runOnUiThreadBlocking(mCondition::check);
                } else {
                    mStatus = mCondition.check();
                }
            } catch (Exception e) {
                mStatus = Condition.error(e.toString());
            }

            return mStatus.isError() || !mStatus.isFulfilled();
        }

        private ConditionStatus getConditionStatus() {
            return mStatus;
        }
    }

    /**
     * Spot checks each of the {@link Condition}s.
     *
     * @param stateName the name of the state whose conditions we are checking.
     * @param conditions the {@link Condition}s to check.
     * @throws AssertionError if not all Conditions are fulfilled.
     */
    public static void check(String stateName, List<Condition> conditions) {
        boolean anyCriteriaMissing = false;
        List<ConditionCheck> checks = new ArrayList<>();
        for (Condition condition : conditions) {
            checks.add(new ConditionCheck(condition));
        }

        for (ConditionCheck check : checks) {
            anyCriteriaMissing |= check.update();
        }

        if (anyCriteriaMissing) {
            throw buildCheckConditionsException(stateName, checks);
        }
    }

    private static AssertionError buildCheckConditionsException(
            String stateName, List<ConditionCheck> checks) {
        return new AssertionError(
                "Preconditions not fulfilled for "
                        + stateName
                        + ":\n"
                        + createCheckConditionsSummary(checks));
    }

    private static String createCheckConditionsSummary(List<ConditionCheck> checks) {
        StringBuilder detailsString = new StringBuilder();

        int i = 1;
        for (ConditionCheck check : checks) {
            String conditionDescription = check.mCondition.getDescription();

            ConditionStatus status = check.getConditionStatus();
            String verdictString =
                    switch (status.getStatus()) {
                        case Status.FULFILLED -> "[OK  ]";
                        case Status.NOT_FULFILLED -> "[FAIL]";
                        case Status.ERROR -> "[ERR ]";
                        default -> null;
                    };

            StringBuilder historyString = new StringBuilder();
            if (status.getMessage() != null) {
                historyString.append("\n        ");
                historyString.append(status.getMessage());
            }

            detailsString
                    .append("    [")
                    .append(i)
                    .append(" ")
                    .append(verdictString)
                    .append(" ")
                    .append(conditionDescription);
            if (historyString.length() > 0) {
                detailsString.append(historyString);
            }
            detailsString.append('\n');
            i++;
        }
        return detailsString.toString();
    }
}
