// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.concurrent.Callable;

/** A {@link Condition} that is checked in the instrumentation thread. */
public abstract class InstrumentationThreadCondition extends Condition {
    public InstrumentationThreadCondition() {
        super(/* isRunOnUiThread= */ false);
    }

    /**
     * Create a simple {@link InstrumentationThreadCondition} that does not need any parameters or
     * to wait on suppliers.
     */
    public static InstrumentationThreadCondition from(
            String description, Callable<ConditionStatus> check) {
        return new InstrumentationThreadCondition() {
            @Override
            public String buildDescription() {
                return description;
            }

            @Override
            public ConditionStatus checkWithSuppliers() {
                try {
                    return check.call();
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        };
    }
}
