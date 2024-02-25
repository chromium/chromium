// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

/** A {@link Condition} that is checked in the UI thread. */
public abstract class UiThreadCondition extends Condition {
    public UiThreadCondition() {
        super(/* isRunOnUiThread= */ true);
    }
}
