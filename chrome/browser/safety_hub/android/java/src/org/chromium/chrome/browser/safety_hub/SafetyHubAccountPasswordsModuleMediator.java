// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.recordDashboardInteractions;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Mediator for the Safety Hub password module. Populates the {@link SafetyHubExpandablePreference}
 * with the user's passwords state, including compromised, weak and reused. It also listens to
 * changes of passwords and their state, and updates the preference to reflect these.
 */
public class SafetyHubAccountPasswordsModuleMediator
        implements SafetyHubModuleMediator,
                SafetyHubFetchService.Observer,
                PasswordStoreBridge.PasswordStoreObserver,
                SigninManager.SignInStateObserver {
    /**
     * This should match the default value for {@link
     * org.chromium.chrome.browser.preferences.Pref.BREACHED_CREDENTIALS_COUNT}.
     */
    private static final int INVALID_BREACHED_CREDENTIALS_COUNT = -1;

    // Represents the type of password module.
    @IntDef({
        ModuleType.SIGNED_OUT,
        ModuleType.UNAVAILABLE_PASSWORDS,
        ModuleType.NO_SAVED_PASSWORDS,
        ModuleType.HAS_COMPROMISED_PASSWORDS,
        ModuleType.NO_COMPROMISED_PASSWORDS,
        ModuleType.HAS_WEAK_PASSWORDS,
        ModuleType.HAS_REUSED_PASSWORDS,
        ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ModuleType {
        int SIGNED_OUT = 0;
        int UNAVAILABLE_PASSWORDS = 1;
        int NO_SAVED_PASSWORDS = 2;
        int HAS_COMPROMISED_PASSWORDS = 3;
        int NO_COMPROMISED_PASSWORDS = 4;
        int HAS_WEAK_PASSWORDS = 5;
        int HAS_REUSED_PASSWORDS = 6;
        int UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS = 7;
    };

    private final Profile mProfile;
    private final PrefService mPrefService;
    private final SafetyHubFetchService mSafetyHubFetchService;
    private final SigninManager mSigninManager;
    private final SafetyHubExpandablePreference mPreference;
    private final SafetyHubModuleDelegate mModuleDelegate;
    private final SafetyHubModuleMediatorDelegate mMediatorDelegate;

    private PasswordStoreBridge mPasswordStoreBridge;
    private PropertyModel mModel;
    private int mCompromisedPasswordsCount;
    private int mWeakPasswordsCount;
    private int mReusedPasswordsCount;
    private boolean mIsSignedIn;
    private @ModuleType int mType;

    SafetyHubAccountPasswordsModuleMediator(
            SafetyHubExpandablePreference preference,
            SafetyHubModuleMediatorDelegate mediatorDelegate,
            SafetyHubModuleDelegate moduleDelegate,
            PrefService prefService,
            SafetyHubFetchService safetyHubFetchService,
            SigninManager signinManager,
            Profile profile) {
        mPreference = preference;
        mPrefService = prefService;
        mSafetyHubFetchService = safetyHubFetchService;
        mMediatorDelegate = mediatorDelegate;
        mModuleDelegate = moduleDelegate;
        mProfile = profile;
        mSigninManager = signinManager;
    }

    @Override
    public void setUpModule() {
        if (isSignedIn()) {
            mPasswordStoreBridge = new PasswordStoreBridge(mProfile);
        }

        mModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mPreference, SafetyHubModuleViewBinder::bindProperties);

        mSafetyHubFetchService.addObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.addObserver(this, true);
        }
        mSigninManager.addSignInStateObserver(this);
        updateState();
    }

    @Override
    public void destroy() {
        mSafetyHubFetchService.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
        }
    }

    @Override
    public void updateStatusChanged() {
        // no-op.
    }

    @Override
    public void passwordCountsChanged() {
        updateModule();
        mMediatorDelegate.onUpdateNeeded();
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        updateModule();
        mMediatorDelegate.onUpdateNeeded();
    }

    @Override
    public void onSignedIn() {
        if (mPasswordStoreBridge == null) {
            mPasswordStoreBridge = new PasswordStoreBridge(mProfile);
            mPasswordStoreBridge.addObserver(this, true);
        }
        updateModule();
        mMediatorDelegate.onUpdateNeeded();
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {}

    @Override
    public void onSignedOut() {
        if (mPasswordStoreBridge != null) {
            mPasswordStoreBridge.removeObserver(this);
            mPasswordStoreBridge = null;
        }
        updateModule();
        mMediatorDelegate.onUpdateNeeded();
    }

    @Override
    public void updateModule() {
        updateState();

        // TODO(https://crbug.com/388788381): Use a factory pattern to fetch the preference fields,
        // such as title, summary, etc.
        switch (mType) {
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
        switch (mType) {
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
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)
                && !passwordSavingEnabled();
    }

    public void triggerNewCredentialFetch() {
        mSafetyHubFetchService.fetchCredentialsCount(success -> {});
    }

    public int getCompromisedPasswordsCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
    }

    public int getWeakPasswordsCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.getInteger(Pref.WEAK_CREDENTIALS_COUNT);
    }

    public int getReusedPasswordsCount() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.getInteger(Pref.REUSED_CREDENTIALS_COUNT);
    }

    public int getTotalPasswordsCount() {
        assert mModuleDelegate != null
                : "A null ModuleDelegate was detected in"
                        + " SafetyHubAccountPasswordsModuleMediator";
        return mModuleDelegate.getAccountPasswordsCount(mPasswordStoreBridge);
    }

    private boolean passwordSavingEnabled() {
        assert mPrefService != null
                : "A null PrefService was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return mPrefService.getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
    }

    private boolean isSignedIn() {
        assert mProfile != null
                : "A null Profile was detected in" + " SafetyHubAccountPasswordsModuleMediator";
        return SafetyHubUtils.isSignedIn(mProfile);
    }

    private void updateState() {
        mCompromisedPasswordsCount = getCompromisedPasswordsCount();
        mWeakPasswordsCount = getWeakPasswordsCount();
        mReusedPasswordsCount = getReusedPasswordsCount();
        mIsSignedIn = isSignedIn();
        mType = getModuleType();
    }

    // Returns the password module type according to the `model` properties.
    private @ModuleType int getModuleType() {
        boolean isWeakAndReusedFeatureEnabled =
                ChromeFeatureList.sSafetyHubWeakAndReusedPasswords.isEnabled();

        if (!mIsSignedIn) {
            assert mCompromisedPasswordsCount == INVALID_BREACHED_CREDENTIALS_COUNT;
            return ModuleType.SIGNED_OUT;
        }
        if (mCompromisedPasswordsCount == INVALID_BREACHED_CREDENTIALS_COUNT) {
            if (isWeakAndReusedFeatureEnabled
                    && mWeakPasswordsCount == 0
                    && mReusedPasswordsCount == 0) {
                return ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS;
            }

            return ModuleType.UNAVAILABLE_PASSWORDS;
        }
        if (getTotalPasswordsCount() == 0) {
            return ModuleType.NO_SAVED_PASSWORDS;
        }
        if (mCompromisedPasswordsCount > 0) {
            return ModuleType.HAS_COMPROMISED_PASSWORDS;
        }
        if (isWeakAndReusedFeatureEnabled) {
            // Reused passwords take priority over the weak passwords count.
            if (mReusedPasswordsCount > 0) {
                return ModuleType.HAS_REUSED_PASSWORDS;
            }
            if (mWeakPasswordsCount > 0) {
                return ModuleType.HAS_WEAK_PASSWORDS;
            }
        }

        // If both reused passwords and weak passwords counts are invalid, ignore them in favour
        // of showing the compromised passwords count.
        return ModuleType.NO_COMPROMISED_PASSWORDS;
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
                                mCompromisedPasswordsCount,
                                mCompromisedPasswordsCount));
        mPreference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                mCompromisedPasswordsCount,
                                mCompromisedPasswordsCount));
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
        String account = SafetyHubUtils.getAccountEmail(mProfile);

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
                                mWeakPasswordsCount,
                                mWeakPasswordsCount));
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
                                mReusedPasswordsCount,
                                mReusedPasswordsCount));
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
        if (mIsSignedIn) {
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
