// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.function.Supplier;

/** Unit Tests for the summary of Conditions printed in TravelExceptions and logs. */
@RunWith(BaseRobolectricTestRunner.class)
public class ConditionsSummaryUnitTest {
    @Rule public final PublicTransitUnitTestRule mTransitRule = new PublicTransitUnitTestRule();

    @Test
    public void testFailedConditionsListedFirstAndGroupedByOrigin() throws Throwable {
        State originState = new FixedNameState("<origin state>");
        originState.declareEnterCondition(new FixedCondition("origin enter ok", true));
        originState.declareExitCondition(new FixedCondition("exit fail", false));
        originState.declareExitCondition(new FixedCondition("exit ok", true));

        State destinationState = new FixedNameState("<destination state>");
        destinationState.declareEnterCondition(new FixedCondition("enter ok", true));
        destinationState.declareEnterCondition(new FixedCondition("enter fail", false));

        mTransitRule.runTransition(
                () -> Triggers.noopTo().withPossiblyAlreadyFulfilled().enterState(originState));

        TravelException exception =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () ->
                                Triggers.noopTo()
                                        .withTimeout(100)
                                        .waitForAnd(new FixedCondition("transition ok", true))
                                        .exitStateAnd(originState)
                                        .enterState(destinationState));
        String message = exception.getMessage();

        // Indices are in Condition creation order: enter, exit, transition.
        assertInOrder(
                message,
                "failed 2 Conditions: [2] enter fail (+1 more)\n\n",
                "❌ FAILED ENTER <destination state>:\n    [2] [FAIL] enter fail",
                "❌ FAILED EXIT <origin state>:\n    [3] [FAIL] exit fail",
                "\n\n✅ OK ENTER <destination state>:\n    [1] [OK  ] enter ok",
                "✅ OK EXIT <origin state>:\n    [4] [OK  ] exit ok",
                "✅ OK TRANSITION:\n    [5] [OK  ] transition ok");
    }

    @Test
    public void testNoFailedBlockWhenAllFulfilled() throws Throwable {
        State destinationState = new FixedNameState("<destination state>");
        destinationState.declareEnterCondition(new FixedCondition("enter ok", true));

        // Throws because all Conditions were already fulfilled.
        Throwable exception =
                mTransitRule.runTransitionExpectingThrow(
                        Throwable.class,
                        () -> Triggers.runTo(() -> {}).enterState(destinationState));
        String messages = getMessagesOfCauseChain(exception);

        assertThat(messages).contains("✅ OK ENTER <destination state>:\n    [1] [OK  ] enter ok");
        assertThat(messages).doesNotContain("FAILED");
    }

    @Test
    public void testConditionsBlockedOnSuppliersReportedAsBlocked() {
        State destinationState = new FixedNameState("<destination state>");
        Element<String> producerElement =
                destinationState.declareEnterConditionAsElement(new NeverResolvingCondition());
        destinationState.declareEnterCondition(
                new DependentCondition("dependent on producer", producerElement, "Product"));
        destinationState.declareEnterCondition(
                new DependentCondition("dependent on outside", () -> null, "OutsideInput"));

        TravelException exception =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () -> Triggers.noopTo().withTimeout(100).enterState(destinationState));
        String message = exception.getMessage();

        assertInOrder(
                message,
                "failed 1 Condition: [1] producer\n\n",
                "❌ FAILED ENTER <destination state>:\n    [1] [FAIL] producer {unfulfilled after",
                "\n\n⏳ BLOCKED ENTER <destination state>:\n"
                        + "    [2] [BLKD] dependent on producer {blocked on [1] for",
                "    [3] [BLKD] dependent on outside {blocked on OutsideInput for");
    }

    private static void assertInOrder(String message, String... expectedSubstrings) {
        int lastIndex = -1;
        for (String expected : expectedSubstrings) {
            int index = message.indexOf(expected);
            assertWithMessage("Expected \"%s\" in:\n%s", expected, message)
                    .that(index)
                    .isAtLeast(0);
            assertWithMessage(
                            "Expected \"%s\" after the previous section in:\n%s", expected, message)
                    .that(index)
                    .isGreaterThan(lastIndex);
            lastIndex = index;
        }
    }

    private static String getMessagesOfCauseChain(Throwable throwable) {
        StringBuilder messages = new StringBuilder();
        for (Throwable t = throwable; t != null; t = t.getCause()) {
            messages.append(t.getMessage()).append('\n');
        }
        return messages.toString();
    }

    /** State with a name without the id, which depends on test order. */
    private static class FixedNameState extends State {
        private final String mFixedName;

        FixedNameState(String fixedName) {
            super(fixedName);
            mFixedName = fixedName;
        }

        @Override
        public String getName() {
            return mFixedName;
        }
    }

    private static class FixedCondition extends Condition {
        private final String mDescription;
        private final boolean mFulfilled;

        FixedCondition(String description, boolean fulfilled) {
            super(/* isRunOnUiThread= */ false);
            mDescription = description;
            mFulfilled = fulfilled;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return mFulfilled ? fulfilled() : notFulfilled();
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }

    private static class NeverResolvingCondition extends ConditionWithResult<String> {
        NeverResolvingCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatusWithResult<String> resolveWithSuppliers() {
            return notFulfilled("not resolved").withoutResult();
        }

        @Override
        public String buildDescription() {
            return "producer";
        }
    }

    private static class DependentCondition extends Condition {
        private final String mDescription;

        DependentCondition(String description, Supplier<?> supplier, String inputName) {
            super(/* isRunOnUiThread= */ false);
            mDescription = description;
            dependOnSupplier(supplier, inputName);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return fulfilled();
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }
}
