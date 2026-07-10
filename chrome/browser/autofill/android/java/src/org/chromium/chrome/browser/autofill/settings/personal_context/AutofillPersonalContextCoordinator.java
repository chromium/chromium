// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.personal_context;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for Autofill Personal Context settings. */
@NullMarked
public class AutofillPersonalContextCoordinator {
    private final AutofillPersonalContextFragment mFragment;
    private final AutofillPersonalContextMediator mMediator;

    private AutofillPersonalContextCoordinator(
            AutofillPersonalContextFragment fragment, Context context, Profile profile) {
        mFragment = fragment;
        mMediator = new AutofillPersonalContextMediator(context, profile);
    }

    public static void createFor(
            AutofillPersonalContextFragment fragment, Context context, Profile profile) {
        AutofillPersonalContextCoordinator coordinator =
                new AutofillPersonalContextCoordinator(fragment, context, profile);
        coordinator.initialize();
    }

    private void initialize() {
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mFragment, AutofillPersonalContextViewBinder::bind);
    }
}
