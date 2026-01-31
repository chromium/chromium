// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A ConditionalState entered when a ViewElement's View has been still for some amount of time. */
@NullMarked
public class ViewSettled extends State {
    ViewSettled(@Nullable Activity activity, ViewElement<?> viewElement) {
        super("ViewSettled");

        RootSpec rootSpec;
        if (activity != null) {
            rootSpec = RootSpec.activityOrDialogRoot(activity);
        } else {
            rootSpec = RootSpec.anyRoot();
        }

        declareView(
                viewElement.getViewSpec(),
                viewElement
                        .copyOptions()
                        .initialSettleTime(1000)
                        .unscoped()
                        .rootSpec(rootSpec)
                        .build());
    }
}
