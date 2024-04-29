// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;
import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog prompting a user to either enable integration with Digital Wellbeing or to revoke
 * permission for that integration. TODO(pnoland): Revisit the style of this dialog and where it's
 * used(i.e. whether it's used from PrivacySettings or not) to ensure that the visual style is
 * consistent.
 */
public class UsageStatsConsentDialog {
    private final Activity mActivity;
    private final Profile mProfile;
    private final boolean mIsRevocation;
    private final Callback<Boolean> mDidConfirmCallback;

    private ModalDialogManager mManager;
    private PropertyModel mDialogModel;

    public static UsageStatsConsentDialog create(
            Activity activity,
            Profile profile,
            boolean isRevocation,
            Callback<Boolean> didConfirmCallback) {
        return new UsageStatsConsentDialog(activity, profile, isRevocation, didConfirmCallback);
    }

    /** Show this dialog in the context of its enclosing activity. */
    public void show() {
        Resources resources = mActivity.getResources();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, makeController())
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel);
        if (mIsRevocation) {
            builder.with(
                            ModalDialogProperties.TITLE,
                            resources,
                            R.string.usage_stats_revocation_prompt)
                    .with(
                            ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                            resources.getString(R.string.usage_stats_revocation_explanation))
                    .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.remove);
        } else {
            builder.with(ModalDialogProperties.TITLE, resources, R.string.usage_stats_consent_title)
                    .with(
                            ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                            resources.getString(R.string.usage_stats_consent_prompt))
                    .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.show);
        }

        mDialogModel = builder.build();
        mManager = new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP);
        mManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private UsageStatsConsentDialog(
            Activity activity,
            Profile profile,
            boolean isRevocation,
            Callback<Boolean> didConfirmCallback) {
        mActivity = activity;
        mProfile = profile;
        mIsRevocation = isRevocation;
        mDidConfirmCallback = didConfirmCallback;
    }

    private ModalDialogProperties.Controller makeController() {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                UsageStatsService service = UsageStatsService.getForProfile(mProfile);
                boolean didConfirm = false;
                switch (buttonType) {
                    case ModalDialogProperties.ButtonType.POSITIVE:
                        didConfirm = true;
                        break;
                    case ModalDialogProperties.ButtonType.NEGATIVE:
                        break;
                }

                if (didConfirm) {
                    service.setOptInState(!mIsRevocation);
                }

                mDidConfirmCallback.onResult(didConfirm);
                dismiss();
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                mDidConfirmCallback.onResult(false);
                mManager.destroy();
            }
        };
    }

    private void dismiss() {
        mManager.destroy();
    }
}
