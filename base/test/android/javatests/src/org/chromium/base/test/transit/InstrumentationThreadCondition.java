// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

/** A {@link Condition} that is checked in the instrumentation thread. */
public abstract class InstrumentationThreadCondition extends Condition {
    public InstrumentationThreadCondition() {
        super(/* isRunOnUiThread= */ false);
    }
}
