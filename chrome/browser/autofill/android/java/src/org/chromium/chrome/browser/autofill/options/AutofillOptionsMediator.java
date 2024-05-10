// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator of the autofill options component. Ensures that the model and the pref are in sync
 * (in either direction).
 */
class AutofillOptionsMediator implements ModalDialogProperties.Controller {
    @VisibleForTesting
    static final String HISTOGRAM_USE_THIRD_PARTY_FILLING =
            "Autofill.Settings.ToggleUseThirdPartyFilling";

    @VisibleForTesting
    static final String HISTOGRAM_REFERRER = "Autofill.Settings.AutofillOptionsReferrerAndroid";

    private final Profile mProfile;
    private final Runnable mRestartRunnable;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<PropertyModel> mRestartConfirmationDialogModelSupplier;
    private PropertyModel mModel;

    AutofillOptionsMediator(
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<PropertyModel> restartConfirmationDialogModelSupplier,
            Runnable restartRunnable) {
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mRestartConfirmationDialogModelSupplier = restartConfirmationDialogModelSupplier;
        mRestartRunnable = restartRunnable;
    }

    // ModalDialogProperties.Controller:
    @Override
    public void onClick(PropertyModel restartConfirmationModel, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                // TODO: crbug.com/308551195 - Add a metric to record acceptance like
                // recordBooleanHistogram(HISTOGRAM_RESTARTED_FOR_3P, true);
                onConfirmWithRestart();
                return;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                // TODO: crbug.com/308551195 - Add a metric to record acceptance like
                // recordBooleanHistogram(HISTOGRAM_RESTARTED_FOR_3P, false);
                mModalDialogManagerSupplier
                        .get()
                        .dismissDialog(
                                restartConfirmationModel,
                                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                return;
            case ModalDialogProperties.ButtonType.TITLE_ICON:
            case ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL:
                assert false : "Unhandled button click!";
        }
    }

    // ModalDialogProperties.Controller:
    @Override
    public void onDismiss(PropertyModel restartConfirmationModel, int dismissalCause) {
        updateToggleStateFromPref(); // Radio buttons always change. Reset them to match the prefs.
    }

    void initialize(PropertyModel model, @AutofillOptionsReferrer int referrer) {
        mModel = model;
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_REFERRER, referrer, AutofillOptionsReferrer.COUNT);
    }

    boolean isInitialized() {
        return mModel != null;
    }

    /**
     * Checks whether the toggle is allowed to switch states. Whenever AwG is the active provider
     * and there is no override, it should not be available to switch over. Switching away from 3P
     * mode when AwG is active is allowed but should never be required since Chrome resets that
     * setting automatically.
     *
     * @return true if the toggle should be read-only.
     */
    boolean should3pToggleBeReadOnly() {
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID,
                "skip_compatibility_check",
                false)) {
            return false;
        }
        return !prefs().getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE)
                && !AutofillClientProviderUtils.isAllowedToUseAndroidAutofillFramework();
    }

    void updateToggleStateFromPref() {
        assert isInitialized();
        mModel.set(
                THIRD_PARTY_AUTOFILL_ENABLED,
                prefs().getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE));
        mModel.set(THIRD_PARTY_TOGGLE_IS_READ_ONLY, should3pToggleBeReadOnly());
    }

    void onThirdPartyToggleChanged(boolean optIntoThirdPartyFilling) {
        if (mModel.get(THIRD_PARTY_AUTOFILL_ENABLED) == optIntoThirdPartyFilling) {
            return; // Ignore redundant event.
        }
        mModel.set(THIRD_PARTY_AUTOFILL_ENABLED, optIntoThirdPartyFilling);
        showRestartConfirmation();
    }

    private void onConfirmWithRestart() {
        prefs().setBoolean(
                        Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE,
                        mModel.get(THIRD_PARTY_AUTOFILL_ENABLED));
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_USE_THIRD_PARTY_FILLING, mModel.get(THIRD_PARTY_AUTOFILL_ENABLED));
        mRestartRunnable.run();
    }

    private void showRestartConfirmation() {
        ModalDialogManager dialogManager = mModalDialogManagerSupplier.get();
        PropertyModel restartConfirmationModel = mRestartConfirmationDialogModelSupplier.get();
        if (dialogManager == null || restartConfirmationModel == null) {
            // Radio buttons always change. Reset them to prefs state if restart can't be confirmed.
            updateToggleStateFromPref();
            return;
        }
        dialogManager.showDialog(restartConfirmationModel, ModalDialogType.APP);
    }

    private PrefService prefs() {
        return UserPrefs.get(mProfile);
    }
}
