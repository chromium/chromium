// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.concurrent.Callable;

/** A {@link Condition} that is checked in the UI thread. */
public abstract class UiThreadCondition extends Condition {
    public UiThreadCondition() {
        super(/* isRunOnUiThread= */ true);
    }

    /**
     * Create a simple {@link UiThreadCondition} that does not need any parameters or to wait on
     * suppliers.
     */
    public static UiThreadCondition from(String description, Callable<ConditionStatus> check) {
        return new UiThreadCondition() {
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
