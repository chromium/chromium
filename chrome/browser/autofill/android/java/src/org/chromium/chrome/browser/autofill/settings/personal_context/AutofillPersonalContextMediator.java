// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.personal_context;

import static org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextProperties.ON_MANAGE_CONNECTED_APPS_CLICKED;
import static org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextProperties.ON_PERSONAL_CONTEXT_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextProperties.PERSONAL_CONTEXT_ENABLED;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for Autofill Personal Context settings. */
@NullMarked
class AutofillPersonalContextMediator {
    private final Context mContext;
    private final Profile mProfile;
    private final PropertyModel mModel;

    AutofillPersonalContextMediator(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;

        mModel =
                new PropertyModel.Builder(AutofillPersonalContextProperties.ALL_KEYS)
                        .with(ON_PERSONAL_CONTEXT_TOGGLE_CHANGED, this::onToggleStatusChanged)
                        .with(ON_MANAGE_CONNECTED_APPS_CLICKED, this::onManageConnectedAppsClicked)
                        .build();

        EntityDataManager manager = EntityDataManagerFactory.getForProfile(mProfile);
        boolean isEnabled = manager != null && manager.isPersonalContextEnabled();
        mModel.set(PERSONAL_CONTEXT_ENABLED, isEnabled);
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onToggleStatusChanged(boolean enabled) {
        EntityDataManager manager = EntityDataManagerFactory.getForProfile(mProfile);
        if (manager != null) {
            manager.setPersonalContextEnabled(enabled);
        }
        // TODO(crbug.com/391094056): The toggle should not change its state if manager is null.
        mModel.set(PERSONAL_CONTEXT_ENABLED, enabled);
        RecordUserAction.record(
                enabled
                        ? AutofillPersonalContextFragment.ACTION_TOGGLED_ON
                        : AutofillPersonalContextFragment.ACTION_TOGGLED_OFF);
    }

    private void onManageConnectedAppsClicked() {
        AutofillUiUtils.openLink(
                mContext, EntityDataManager.getPersonalContextManageConnectedAppsUrl());
        RecordUserAction.record(AutofillPersonalContextFragment.ACTION_MANAGE_CONNECTED_APPS);
    }
}
