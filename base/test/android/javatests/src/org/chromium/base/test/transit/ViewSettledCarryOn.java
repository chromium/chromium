// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A ConditionalState entered when a ViewElement's View has been still for some amount of time. */
@NullMarked
class ViewSettledCarryOn extends CarryOn {
    ViewSettledCarryOn(
            @Nullable ActivityElement<?> otherActivityElement, ViewElement<?> viewElement) {
        super("ViewSettled");

        if (otherActivityElement != null) {
            Class<? extends Activity> activityClass = otherActivityElement.getActivityClass();
            Activity activity = otherActivityElement.get();
            assert activityClass != null;
            assert activity != null;

            declareActivity(activityClass).requireToBeInSameTask(activity);
        }

        declareView(
                viewElement.getViewSpec(),
                viewElement.copyOptions().initialSettleTime(1000).unscoped().build());
    }
}
