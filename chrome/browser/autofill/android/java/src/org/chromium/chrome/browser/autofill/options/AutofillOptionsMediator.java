// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.FRAGMENT_TITLE;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_AUTOFILL_AI_REAUTH_SETTING_TOGGLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_AUTOFILL_AI_SETTING_TOGGLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_HINT;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_TOGGLE_IS_READ_ONLY;

import android.app.Activity;
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
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.AutofillAiOptInStatus;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.function.Supplier;

/**
 * The mediator of the autofill options component. Ensures that the model and the pref are in sync
 * (in either direction).
 */
@NullMarked
class AutofillOptionsMediator implements ModalDialogProperties.Controller {
    private static final String NON_PACKAGE_NAME = "package:not.a.package.so.all.providers.show";

    @VisibleForTesting
    static final String HISTOGRAM_USE_THIRD_PARTY_FILLING =
            "Autofill.Settings.ToggleUseThirdPartyFilling";

    @VisibleForTesting
    static final String HISTOGRAM_REFERRER = "Autofill.Settings.AutofillOptionsReferrerAndroid";

    @VisibleForTesting
    static final String HISTOGRAM_RESTART_ACCEPTED =
            "Autofill.Settings.AutofillOptionsRestartAccepted";

    private final Profile mProfile;
    private final Runnable mRestartRunnable;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<PropertyModel> mRestartConfirmationDialogModelSupplier;
    private PropertyModel mModel;
    private Context mContext;
    private Activity mActivity;
    private @Nullable ReauthenticatorBridge mReauthenticatorBridge;

    AutofillOptionsMediator(
            Profile profile,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
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
                RecordHistogram.recordBooleanHistogram(HISTOGRAM_RESTART_ACCEPTED, true);
                onConfirmWithRestart();
                return;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                RecordHistogram.recordBooleanHistogram(HISTOGRAM_RESTART_ACCEPTED, false);
                ModalDialogManager dialogManager = mModalDialogManagerSupplier.get();
                assumeNonNull(dialogManager);
                dialogManager.dismissDialog(
                        restartConfirmationModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                return;
            case ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL:
                assert false : "Unhandled button click!";
        }
    }

    // ModalDialogProperties.Controller:
    @Override
    public void onDismiss(PropertyModel restartConfirmationModel, int dismissalCause) {
        updateToggleStateFromPref(); // Radio buttons always change. Reset them to match the prefs.
    }

    @Initializer
    void initialize(@AutofillOptionsReferrer int referrer, Context context, Activity activity) {
        mContext = context;
        mActivity = activity;
        mModel =
                new PropertyModel.Builder(AutofillOptionsProperties.ALL_KEYS)
                        .with(FRAGMENT_TITLE, getFragmentTitle())
                        .with(ON_THIRD_PARTY_TOGGLE_CHANGED, this::onThirdPartyToggleChanged)
                        .with(ON_AUTOFILL_AI_SETTING_TOGGLED, this::onAutofillAiSettingToggled)
                        .with(
                                ON_AUTOFILL_AI_REAUTH_SETTING_TOGGLED,
                                this::onAutofillAiReauthSettingToggled)
                        .build();
        updateToggleStateFromPref();
        mModel.set(AutofillOptionsProperties.AUTOFILL_AI_VISIBLE, isAutofillAiVisible(referrer));
        mModel.set(
                AutofillOptionsProperties.AUTOFILL_AI_SETTING_ELIGIBLE, isEligibleToAutofillAi());
        mModel.set(AutofillOptionsProperties.AUTOFILL_AI_SETTING_ON, isAutofillAiOn());
        mModel.set(AutofillOptionsProperties.AUTOFILL_AI_REAUTH_SETTING_ON, isAutofillAiReauthOn());
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_REFERRER, referrer, AutofillOptionsReferrer.COUNT);
    }

    void destroy() {
        if (mReauthenticatorBridge != null) {
            mReauthenticatorBridge.destroy();
            mReauthenticatorBridge = null;
        }
    }

    boolean isInitialized() {
        return mModel != null;
    }

    PropertyModel getModel() {
        return mModel;
    }

    private String getFragmentTitle() {
        return isAutofillAiEnabled()
                ? mContext.getString(R.string.autofill_settings_title)
                : mContext.getString(R.string.autofill_options_title);
    }

    private boolean isAutofillAiVisible(@AutofillOptionsReferrer int referrer) {
        // Autofill AI related preferences are not shown if the fragment is opened using a deep
        // link to show only the 3p Autofill services toggle.
        return referrer != AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS && isAutofillAiEnabled();
    }

    private boolean isAutofillAiEnabled() {
        // LINT.IfChange(AutofillEnabledCheckMediator)
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA);
        // LINT.ThenChange(AutofillEnabledCheckFragment)
    }

    private boolean isEligibleToAutofillAi() {
        @Nullable EntityDataManager manager = EntityDataManagerFactory.getForProfile(mProfile);
        return isAutofillAiEnabled() && manager != null && manager.isEligibleToAutofillAi();
    }

    private boolean isAutofillAiOn() {
        @Nullable EntityDataManager manager = EntityDataManagerFactory.getForProfile(mProfile);
        return isAutofillAiEnabled() && manager != null && manager.getAutofillAiOptInStatus();
    }

    private void onAutofillAiSettingToggled(boolean isOn) {
        @AutofillAiOptInStatus
        int optInStatus = isOn ? AutofillAiOptInStatus.OPTED_IN : AutofillAiOptInStatus.OPTED_OUT;
        @Nullable EntityDataManager manager = EntityDataManagerFactory.getForProfile(mProfile);
        if (manager == null || !manager.setAutofillAiOptInStatus(optInStatus)) {
            // If failed to set, reset the switch to match current status.
            mModel.set(AutofillOptionsProperties.AUTOFILL_AI_SETTING_ON, isAutofillAiOn());
        }
    }

    private boolean isAutofillAiReauthOn() {
        return prefs().getBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA);
    }

    private void onAutofillAiReauthSettingToggled(boolean isOn) {
        if (isOn == isAutofillAiReauthOn()) {
            return;
        }

        if (mReauthenticatorBridge == null) {
            mReauthenticatorBridge =
                    ReauthenticatorBridge.create(mActivity, mProfile, DeviceAuthSource.AUTOFILL);
        }

        if (mReauthenticatorBridge.getBiometricAvailabilityStatus()
                == BiometricStatus.UNAVAILABLE) {
            prefs().setBoolean(Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA, isOn);
            mModel.set(
                    AutofillOptionsProperties.AUTOFILL_AI_REAUTH_SETTING_ON,
                    isAutofillAiReauthOn());
            return;
        }

        mReauthenticatorBridge.reauthenticate(
                (success) -> {
                    if (success) {
                        prefs().setBoolean(
                                        Pref.AUTOFILL_AI_REAUTH_BEFORE_VIEWING_SENSITIVE_DATA,
                                        isOn);
                    }
                    // Always sync the model to either the new value or back to the old one on
                    // failure.
                    mModel.set(
                            AutofillOptionsProperties.AUTOFILL_AI_REAUTH_SETTING_ON,
                            isAutofillAiReauthOn());
                });
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
        if (prefs().getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL)) {
            return false; // Always allow to flip back to built-in password management.
        }
        switch (AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(prefs())) {
            case AndroidAutofillAvailabilityStatus.NOT_ALLOWED_BY_POLICY:
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_MANAGER_NOT_AVAILABLE:
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_NOT_SUPPORTED:
            case AndroidAutofillAvailabilityStatus.UNKNOWN_ANDROID_AUTOFILL_SERVICE:
            case AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_SERVICE_IS_GOOGLE:
                return true;
            case AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF: // Pref may be changed!
            case AndroidAutofillAvailabilityStatus.AVAILABLE:
                return false;
        }
        assert false : "Unhandled AndroidAutofillFrameworkAvailability state!";
        return false;
    }

    void updateToggleStateFromPref() {
        assert isInitialized();
        mModel.set(
                THIRD_PARTY_AUTOFILL_ENABLED,
                prefs().getBoolean(Pref.AUTOFILL_USING_PLATFORM_AUTOFILL));
        mModel.set(THIRD_PARTY_TOGGLE_IS_READ_ONLY, should3pToggleBeReadOnly());
        mModel.set(THIRD_PARTY_TOGGLE_HINT, getHintSummary());
    }

    private void onThirdPartyToggleChanged(boolean optIntoThirdPartyFilling) {
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
                        new ChromeClickableSpan(mContext, this::onLinkToAndroidSettingsClicked)));
    }

    private void onLinkToAndroidSettingsClicked(View unusedView) {
        IntentUtils.safeStartActivity(mContext, createAutofillServiceChangeIntent());
    }

    private static Intent createAutofillServiceChangeIntent() {
        Intent intent = new Intent(Settings.ACTION_REQUEST_SET_AUTOFILL_SERVICE);
        // Request an unlikely package to become the new provider to ensure the picker always shows.
        intent.setData(Uri.parse(NON_PACKAGE_NAME));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    private void onConfirmWithRestart() {
        prefs().setBoolean(
                        Pref.AUTOFILL_USING_PLATFORM_AUTOFILL,
                        mModel.get(THIRD_PARTY_AUTOFILL_ENABLED));
        AutofillClientProviderUtils.updatePackageUsedForAutofill(
                prefs(), mModel.get(THIRD_PARTY_AUTOFILL_ENABLED));
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
        return mContext.getString(stringRes);
    }
}
