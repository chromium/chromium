// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.safety_hub.SafetyHubAccountPasswordsDataSource.ModuleType;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Mediator for the Safety Hub password module. Populates the {@link SafetyHubExpandablePreference}
 * with the user's passwords state, including compromised, weak and reused. It gets notified of
 * changes of passwords and their state by {@link SafetyHubAccountPasswordsDataSource}, and updates
 * the preference to reflect these.
 */
public class SafetyHubAccountPasswordsModuleMediator
        implements SafetyHubModuleMediator, SafetyHubAccountPasswordsDataSource.Observer {
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;

    private SafetyHubAccountPasswordsDataSource mAccountPasswordsDataSource;
    private PropertyModel mModel;
    private @ModuleType int mModuleType;

    SafetyHubAccountPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubAccountPasswordsDataSource accountPasswordsDataSource,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate) {
        mPreference = preference;
        mAccountPasswordsDataSource = accountPasswordsDataSource;
        mMediatorDelegate = mediatorDelegate;
        mModuleDelegate = moduleDelegate;
    }

    @Override
    public void setUpModule() {
        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mAccountPasswordsDataSource.setObserver(this);
        mAccountPasswordsDataSource.setUp();
    }

    @Override
    public void destroy() {
        if (mAccountPasswordsDataSource != null) {
            mAccountPasswordsDataSource.destroy();
            mAccountPasswordsDataSource = null;
        }
    }

    @Override
    public void updateModule() {
        mAccountPasswordsDataSource.updateState();
    }

    private void updateModule(@ModuleType int moduleType) {
        mModuleType = moduleType;
        switch (mModuleType) {
            case ModuleType.SIGNED_OUT:
                updatePreferenceForSignedOut();
                break;
            case ModuleType.UNAVAILABLE_PASSWORDS:
                updatePreferenceForUnavailablePasswords();
                break;
            case ModuleType.NO_SAVED_PASSWORDS:
                updatePreferenceForNoSavedPasswords();
                break;
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                updatePreferenceForHasCompromisedPasswords();
                break;
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                updatePreferenceForNoCompromisedPasswords();
                break;
            case ModuleType.HAS_WEAK_PASSWORDS:
                updatePreferenceForHasWeakPasswords();
                break;
            case ModuleType.HAS_REUSED_PASSWORDS:
                updatePreferenceForHasReusedPasswords();
                break;
            case ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS:
                updatePreferenceForUnavailableCompromisedNoWeakReusePasswords();
                break;
            default:
                throw new IllegalArgumentException();
        }

        if (isManaged()) {
            overridePreferenceForManaged();
        }

        mModel.set(SafetyHubModuleProperties.ORDER, getOrder());
        mModel.set(SafetyHubModuleProperties.ICON, getIcon(mPreference.getContext()));
    }

    @Override
    public void setExpandState(boolean expanded) {
        mModel.set(SafetyHubModuleProperties.IS_EXPANDED, expanded);
    }

    @Override
    public @ModuleState int getModuleState() {
        switch (mModuleType) {
            case ModuleType.NO_SAVED_PASSWORDS:
            case ModuleType.HAS_WEAK_PASSWORDS:
            case ModuleType.HAS_REUSED_PASSWORDS:
                return ModuleState.INFO;
            case ModuleType.SIGNED_OUT:
            case ModuleType.UNAVAILABLE_PASSWORDS:
            case ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS:
                return ModuleState.UNAVAILABLE;
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                return ModuleState.WARNING;
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                return ModuleState.SAFE;
            default:
                throw new IllegalArgumentException();
        }
    }

    @Override
    public @ModuleOption int getOption() {
        return ModuleOption.ACCOUNT_PASSWORDS;
    }

    @Override
    public boolean isManaged() {
        return mAccountPasswordsDataSource.isManaged();
    }

    public void triggerNewCredentialFetch() {
        mAccountPasswordsDataSource.triggerNewCredentialFetch();
    }

    @Override
    public void stateChanged(@ModuleType int moduleType) {
        updateModule(moduleType);
        mMediatorDelegate.onUpdateNeeded();
    }

    // Updates `preference` for the password module of type {@link ModuleType.SIGNED_OUT}.
    private void updatePreferenceForSignedOut() {
        Context context = mPreference.getContext();
        mPreference.setTitle(
                context.getString(R.string.safety_hub_password_check_unavailable_title));
        mPreference.setSummary(
                context.getString(R.string.safety_hub_password_check_signed_out_summary));
        mPreference.setPrimaryButtonText(null);
        mPreference.setPrimaryButtonClickListener(null);
        mPreference.setSecondaryButtonText(context.getString(R.string.sign_in_to_chrome));
        mPreference.setSecondaryButtonClickListener(getButtonListener());
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.UNAVAILABLE_PASSWORDS}.
    private void updatePreferenceForUnavailablePasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(
                context.getString(R.string.safety_hub_password_check_unavailable_title));
        mPreference.setSummary(context.getString(R.string.safety_hub_unavailable_summary));
        mPreference.setPrimaryButtonText(null);
        mPreference.setPrimaryButtonClickListener(null);
        mPreference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setSecondaryButtonClickListener(getButtonListener());
    }

    // Updates `preference` for the password module of type {@link ModuleType.NO_SAVED_PASSWORDS}.
    private void updatePreferenceForNoSavedPasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(context.getString(R.string.safety_hub_no_passwords_title));
        mPreference.setSummary(context.getString(R.string.safety_hub_no_passwords_summary));
        mPreference.setPrimaryButtonText(null);
        mPreference.setPrimaryButtonClickListener(null);
        mPreference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setSecondaryButtonClickListener(getButtonListener());
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.HAS_COMPROMISED_PASSWORDS}.
    private void updatePreferenceForHasCompromisedPasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                mAccountPasswordsDataSource.getCompromisedPasswordCount(),
                                mAccountPasswordsDataSource.getCompromisedPasswordCount()));
        mPreference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                mAccountPasswordsDataSource.getCompromisedPasswordCount(),
                                mAccountPasswordsDataSource.getCompromisedPasswordCount()));
        mPreference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setPrimaryButtonClickListener(getButtonListener());
        mPreference.setSecondaryButtonText(null);
        mPreference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.NO_COMPROMISED_PASSWORDS}.
    private void updatePreferenceForNoCompromisedPasswords() {
        Context context = mPreference.getContext();
        String account = mAccountPasswordsDataSource.getAccountEmail();

        mPreference.setTitle(context.getString(R.string.safety_hub_no_compromised_passwords_title));

        if (account != null) {
            mPreference.setSummary(
                    context.getString(R.string.safety_hub_password_check_time_recently, account));
        } else {
            mPreference.setSummary(context.getString(R.string.safety_hub_checked_recently));
        }
        mPreference.setPrimaryButtonText(null);
        mPreference.setPrimaryButtonClickListener(null);
        mPreference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setSecondaryButtonClickListener(getButtonListener());
    }

    // Updates `preference` for the password module of type {@link ModuleType.HAS_WEAK_PASSWORDS}.
    private void updatePreferenceForHasWeakPasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(context.getString(R.string.safety_hub_reused_weak_passwords_title));
        mPreference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                mAccountPasswordsDataSource.getWeakPasswordCount(),
                                mAccountPasswordsDataSource.getWeakPasswordCount()));
        mPreference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setPrimaryButtonClickListener(getButtonListener());
        mPreference.setSecondaryButtonText(null);
        mPreference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link ModuleType.HAS_REUSED_PASSWORDS}.
    private void updatePreferenceForHasReusedPasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(context.getString(R.string.safety_hub_reused_weak_passwords_title));
        mPreference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                mAccountPasswordsDataSource.getReusedPasswordCount(),
                                mAccountPasswordsDataSource.getReusedPasswordCount()));
        mPreference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setPrimaryButtonClickListener(getButtonListener());
        mPreference.setSecondaryButtonText(null);
        mPreference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS}.
    private void updatePreferenceForUnavailableCompromisedNoWeakReusePasswords() {
        Context context = mPreference.getContext();
        mPreference.setTitle(context.getString(R.string.safety_hub_no_reused_weak_passwords_title));
        mPreference.setSummary(
                context.getString(
                        R.string
                                .safety_hub_unavailable_compromised_no_reused_weak_passwords_summary));
        mPreference.setPrimaryButtonText(null);
        mPreference.setPrimaryButtonClickListener(null);
        mPreference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        mPreference.setSecondaryButtonClickListener(getButtonListener());
    }

    // Overrides summary and primary button fields of `preference` if passwords are controlled by a
    // policy.
    private void overridePreferenceForManaged() {
        assert isManaged();
        mPreference.setSummary(
                mPreference
                        .getContext()
                        .getString(R.string.safety_hub_no_passwords_summary_managed));
        String primaryButtonText = mPreference.getPrimaryButtonText();
        View.OnClickListener primaryButtonListener = mPreference.getPrimaryButtonClickListener();
        if (primaryButtonText != null) {
            assert mPreference.getSecondaryButtonText() == null;
            mPreference.setSecondaryButtonText(primaryButtonText);
            mPreference.setSecondaryButtonClickListener(primaryButtonListener);
            mPreference.setPrimaryButtonText(null);
            mPreference.setPrimaryButtonClickListener(null);
        }
    }

    private View.OnClickListener getButtonListener() {
        if (mAccountPasswordsDataSource.isSignedIn()) {
            return v -> {
                mModuleDelegate.showPasswordCheckUi(mPreference.getContext());
                recordDashboardInteractions(DashboardInteractions.OPEN_PASSWORD_MANAGER);
            };
        } else {
            return v -> {
                mModuleDelegate.launchSigninPromo(mPreference.getContext());
                recordDashboardInteractions(DashboardInteractions.SHOW_SIGN_IN_PROMO);
            };
        }
    }
}
