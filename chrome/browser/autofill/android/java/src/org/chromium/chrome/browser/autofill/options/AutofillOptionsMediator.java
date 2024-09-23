// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_HINT;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.R;
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
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * The mediator of the autofill options component. Ensures that the model and the pref are in sync
 * (in either direction).
 */
class AutofillOptionsMediator implements ModalDialogProperties.Controller {
    private static final String AWG_PACKAGE_NAME = "package:com.google.android.gms";
    private static final String SKIP_COMPATIBILITY_CHECK_PARAM_NAME = "skip_compatibility_check";
    private static final String SKIP_ALL_CHECKS_PARAM_VALUE = "skip_all_checks";
    private static final String ONLY_SKIP_AWG_CHECK_PARAM_VALUE = "only_skip_awg_check";

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
    private Context mContext;

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

    void initialize(PropertyModel model, @AutofillOptionsReferrer int referrer, Context context) {
        mModel = model;
        mContext = context;
        updateToggleStateFromPref();
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
        if (prefs().getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE)) {
            return false; // Always allow to flip back to built-in password management.
        }
        switch (AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(prefs())) {
            case AndroidAutofillAvailabilityStatus.NOT_ALLOWED_BY_POLICY:
                return true;
            case AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF: // Pref may be changed!
            case AndroidAutofillAvailabilityStatus.AVAILABLE:
                return false;
            case AndroidAutofillAvailabilityStatus.ANDROID_VERSION_TOO_OLD:
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_MANAGER_NOT_AVAILABLE:
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_NOT_SUPPORTED:
            case AndroidAutofillAvailabilityStatus.UNKNOWN_ANDROID_AUTOFILL_SERVICE:
                return !SKIP_ALL_CHECKS_PARAM_VALUE.equals(
                        ChromeFeatureList.getFieldTrialParamByFeature(
                                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID,
                                SKIP_COMPATIBILITY_CHECK_PARAM_NAME));
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_SERVICE_IS_GOOGLE:
                return !SKIP_ALL_CHECKS_PARAM_VALUE.equals(
                                ChromeFeatureList.getFieldTrialParamByFeature(
                                        ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID,
                                        SKIP_COMPATIBILITY_CHECK_PARAM_NAME))
                        && !ONLY_SKIP_AWG_CHECK_PARAM_VALUE.equals(
                                ChromeFeatureList.getFieldTrialParamByFeature(
                                        ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID,
                                        SKIP_COMPATIBILITY_CHECK_PARAM_NAME));
        }
        assert false : "Unhandled AndroidAutofillFrameworkAvailability state!";
        return false;
    }

    void updateToggleStateFromPref() {
        assert isInitialized();
        mModel.set(
                THIRD_PARTY_AUTOFILL_ENABLED,
                prefs().getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE));
        mModel.set(THIRD_PARTY_TOGGLE_IS_READ_ONLY, should3pToggleBeReadOnly());
        mModel.set(THIRD_PARTY_TOGGLE_HINT, getHintSummary());
    }

    void onThirdPartyToggleChanged(boolean optIntoThirdPartyFilling) {
        if (mModel.get(THIRD_PARTY_AUTOFILL_ENABLED) == optIntoThirdPartyFilling) {
            return; // Ignore redundant event.
        }
        mModel.set(THIRD_PARTY_AUTOFILL_ENABLED, optIntoThirdPartyFilling);
        showRestartConfirmation();
    }

    private SpannableString getHintSummary() {
        if (AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(prefs())
                == AndroidAutofillAvailabilityStatus.NOT_ALLOWED_BY_POLICY) {
            return SpannableString.valueOf(getString(R.string.autofill_options_hint_policy));
        }
        return SpanApplier.applySpans(
                getString(
                        should3pToggleBeReadOnly()
                                ? R.string.autofill_options_hint_3p_setting_disabled
                                : R.string.autofill_options_hint_3p_setting_ready),
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new NoUnderlineClickableSpan(
                                mContext, this::onLinkToAndroidSettingsClicked)));
    }

    private void onLinkToAndroidSettingsClicked(View unusedView) {
        IntentUtils.safeStartActivity(mContext, createAutofillServiceChangeIntent());
    }

    private static Intent createAutofillServiceChangeIntent() {
        Intent intent = new Intent(Settings.ACTION_REQUEST_SET_AUTOFILL_SERVICE);
        intent.setData(Uri.parse(AWG_PACKAGE_NAME));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
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

    private String getString(@StringRes int stringRes) {
        return mContext.getResources().getString(stringRes);
    }
}
