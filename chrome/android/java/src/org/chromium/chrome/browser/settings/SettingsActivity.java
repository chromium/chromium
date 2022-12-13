// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ApplicationLifetime;
import org.chromium.chrome.browser.BackPressHelper;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentBasic;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUiFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckFragmentView;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditUiFactory;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
import org.chromium.chrome.browser.privacy_sandbox.AdMeasurementFragment;
import org.chromium.chrome.browser.privacy_sandbox.AdPersonalizationFragment;
import org.chromium.chrome.browser.privacy_sandbox.AdPersonalizationRemovedFragment;
import org.chromium.chrome.browser.privacy_sandbox.FlocSettingsFragment;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.safety_check.SafetyCheckCoordinator;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckUpdatesDelegateImpl;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The Chrome settings activity.
 *
 * This activity displays a single {@link Fragment}, typically a {@link PreferenceFragmentCompat}.
 * As the user navigates through settings, a separate Settings activity is created for each
 * screen. Thus each fragment may freely modify its activity's action bar or title. This mimics the
 * behavior of {@link android.preference.PreferenceActivity}.
 */
public class SettingsActivity extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback, SnackbarManageable {
    /**
     * Preference fragments may implement this interface to intercept "Back" button taps in this
     * activity.
     */
    public interface OnBackPressedListener {
        /**
         * Called when the user taps "Back".
         * @return Whether "Back" button was handled by the fragment. If this method returns false,
         *         the activity should handle the event itself.
         */
        boolean onBackPressed();
    }
    static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";

    /** The current instance of SettingsActivity in the resumed state, if any. */
    private static SettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    /** An instance of settings launcher that can be injected into a fragment */
    private SettingsLauncher mSettingsLauncher = new SettingsLauncherImpl();

    private SnackbarManager mSnackbarManager;

    private ScrimCoordinator mScrim;

    private BottomSheetController mBottomSheetController;

    @Nullable
    private UiConfig mUiConfig;

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTitle(R.string.settings);
        ensureActivityNotExported();

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Settings after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        super.onCreate(savedInstanceState);

        setContentView(R.layout.settings_activity);

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        mIsNewlyCreated = savedInstanceState == null;

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            if (initialFragment == null) initialFragment = MainSettings.class.getName();

            Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);
            getSupportFragmentManager().beginTransaction().replace(R.id.content, fragment).commit();
        }

        // Set width constraints
        configureWideDisplayStyle();
        setStatusBarColor();
        initBottomSheet();
        BackPressHelper.create(this, getOnBackPressedDispatcher(), this::handleBackPressed);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Set width constraints
        configureWideDisplayStyle();
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered
     * by adding padding to both sides.
     */
    private void configureWideDisplayStyle() {
        if (mUiConfig == null) {
            int minWidePaddingPixels =
                    getResources().getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);
            View view = findViewById(R.id.content);
            mUiConfig = new UiConfig(view);
            ViewResizer.createAndAttach(view, mUiConfig, 0, minWidePaddingPixels);
        } else {
            mUiConfig.updateDisplayStyle();
        }
    }

    /** Set up the bottom sheet for this activity. */
    private void initBottomSheet() {
        ViewGroup sheetContainer = findViewById(R.id.sheet_container);
        mScrim =
                new ScrimCoordinator(this, new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {
                        // TODO: Implement if status bar needs to change color with the scrim.
                    }

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {
                        // TODO: Implement if navigation bar needs to change color with the scrim.
                    }
                }, (ViewGroup) sheetContainer.getParent(), getColor(R.color.default_scrim_color));

        // clang-format off
        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                () -> mScrim, (sheet) -> {}, getWindow(),
                KeyboardVisibilityDelegate.getInstance(), () -> sheetContainer);
        // clang-format on
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        startFragment(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new Settings activity showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startFragment(String fragmentClass, Bundle args) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClass(this, getClass());
        intent.putExtra(EXTRA_SHOW_FRAGMENT, fragmentClass);
        intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        ViewGroup contentView = findViewById(android.R.id.content);
        mSnackbarManager = new SnackbarManager(this, contentView, null);

        Fragment fragment = getMainFragment();

        if (fragment instanceof SiteSettingsPreferenceFragment) {
            ChromeSiteSettingsDelegate delegate =
                    (ChromeSiteSettingsDelegate) (((SiteSettingsPreferenceFragment) fragment)
                                                          .getSiteSettingsDelegate());
            delegate.setSnackbarManager(mSnackbarManager);
        }
        if (fragment instanceof AdPersonalizationFragment) {
            ((AdPersonalizationFragment) fragment).setSnackbarManager(getSnackbarManager());
        }
        if (fragment instanceof AdPersonalizationRemovedFragment) {
            ((AdPersonalizationRemovedFragment) fragment).setSnackbarManager(getSnackbarManager());
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of SettingsActivity at the same
        // time (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()
                && !mIsNewlyCreated) {
            // This activity was unpaused or recreated while another instance of SettingsActivity
            // was already showing. The existing instance takes precedence.
            finish();
        } else {
            // This activity was newly created and takes precedence over sResumedInstance.
            if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
                sResumedInstance.finish();
            }

            sResumedInstance = this;
            mIsNewlyCreated = false;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        ProfileManagerUtils.flushPersistentDataForAllProfiles();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (sResumedInstance == this) sResumedInstance = null;
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a {@link
     * PreferenceFragmentCompat}. This does not include dialogs or other {@link Fragment}s shown on
     * top of the main content.
     */
    @VisibleForTesting
    public Fragment getMainFragment() {
        return getSupportFragmentManager().findFragmentById(R.id.content);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_general_help, Menu.CATEGORY_SECONDARY, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getTheme()));
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        if (menu.size() == 1) {
            MenuItem item = menu.getItem(0);
            if (item.getIcon() != null) item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        Fragment mainFragment = getMainFragment();
        if (mainFragment != null && mainFragment.onOptionsItemSelected(item)) {
            return true;
        }

        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(this,
                    getString(R.string.help_context_settings), Profile.getLastUsedRegularProfile(),
                    null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private boolean handleBackPressed() {
        Fragment activeFragment = getMainFragment();
        if (!(activeFragment instanceof OnBackPressedListener)) return false;
        OnBackPressedListener listener = (OnBackPressedListener) activeFragment;
        return listener.onBackPressed();
    }

    @Override
    public void onAttachFragment(Fragment fragment) {
        if (fragment instanceof MainSettings) {
            ((MainSettings) fragment)
                    .setModalDialogManagerSupplier(getModalDialogManagerSupplier());
        }
        if (fragment instanceof SiteSettingsPreferenceFragment) {
            ((SiteSettingsPreferenceFragment) fragment)
                    .setSiteSettingsDelegate(new ChromeSiteSettingsDelegate(
                            this, Profile.getLastUsedRegularProfile()));
        }
        if (fragment instanceof FragmentSettingsLauncher) {
            FragmentSettingsLauncher fragmentSettingsLauncher = (FragmentSettingsLauncher) fragment;
            fragmentSettingsLauncher.setSettingsLauncher(mSettingsLauncher);
        }
        if (fragment instanceof FragmentHelpAndFeedbackLauncher) {
            FragmentHelpAndFeedbackLauncher fragmentHelpAndFeedbackLauncher =
                    (FragmentHelpAndFeedbackLauncher) fragment;
            fragmentHelpAndFeedbackLauncher.setHelpAndFeedbackLauncher(
                    HelpAndFeedbackLauncherImpl.getInstance());
        }
        if (fragment instanceof SafetyCheckSettingsFragment) {
            SafetyCheckCoordinator.create((SafetyCheckSettingsFragment) fragment,
                    new SafetyCheckUpdatesDelegateImpl(), mSettingsLauncher,
                    SyncConsentActivityLauncherImpl.get(), getModalDialogManagerSupplier());
        }
        if (fragment instanceof PasswordCheckFragmentView) {
            PasswordCheckComponentUiFactory.create((PasswordCheckFragmentView) fragment,
                    HelpAndFeedbackLauncherImpl.getInstance(), mSettingsLauncher,
                    LaunchIntentDispatcher::createCustomTabActivityIntent,
                    IntentUtils::addTrustedIntentExtras);
        }
        if (fragment instanceof CredentialEntryFragmentViewBase) {
            CredentialEditUiFactory.create((CredentialEntryFragmentViewBase) fragment,
                    HelpAndFeedbackLauncherImpl.getInstance());
        }
        if (fragment instanceof SearchEngineSettings) {
            SearchEngineSettings settings = (SearchEngineSettings) fragment;
            settings.setDisableAutoSwitchRunnable(
                    () -> LocaleManager.getInstance().setSearchEngineAutoSwitch(false));
            settings.setSettingsLauncher(mSettingsLauncher);
        }
        if (fragment instanceof ImageDescriptionsSettings) {
            Profile profile = Profile.getLastUsedRegularProfile();
            ImageDescriptionsSettings imageFragment = (ImageDescriptionsSettings) fragment;
            Bundle extras = imageFragment.getArguments();
            if (extras != null) {
                extras.putBoolean(ImageDescriptionsSettings.IMAGE_DESCRIPTIONS,
                        ImageDescriptionsController.getInstance().imageDescriptionsEnabled(
                                profile));
                extras.putBoolean(ImageDescriptionsSettings.IMAGE_DESCRIPTIONS_DATA_POLICY,
                        ImageDescriptionsController.getInstance().onlyOnWifiEnabled(profile));
            }
            imageFragment.setDelegate(ImageDescriptionsController.getInstance().getDelegate());
        }
        if (fragment instanceof PrivacySandboxSettingsBaseFragment) {
            ((PrivacySandboxSettingsBaseFragment) fragment)
                    .setCustomTabIntentHelper(
                            LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AdMeasurementFragment) {
            // Unlike HistoryManagerUtils, which opens History in a tab on Tablets, this always
            // opens history in a new activity on top of the SettingsActivity.
            Runnable openHistoryRunnable = () -> {
                // TODO(crbug.com/1286276): Opening History overrides the last active tab. Fix it.
                Activity activity = fragment.getActivity();
                Intent intent = new Intent();
                intent.setClass(activity, HistoryActivity.class);
                intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, false);
                activity.startActivity(intent);
            };
            ((AdMeasurementFragment) fragment).setSetHistoryHelper(openHistoryRunnable);
        }
        if (fragment instanceof FlocSettingsFragment) {
            ((FlocSettingsFragment) fragment)
                    .setCustomTabIntentHelper(
                            LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof LanguageSettings) {
            ((LanguageSettings) fragment).setRestartAction(() -> {
                ApplicationLifetime.terminate(true);
            });
        }
        if (fragment instanceof ClearBrowsingDataFragmentBasic) {
            ((ClearBrowsingDataFragmentBasic) fragment)
                    .setCustomTabIntentHelper(
                            LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof PrivacyGuideFragment) {
            ((PrivacyGuideFragment) fragment).setBottomSheetController(mBottomSheetController);
        }
        if (fragment instanceof AccessibilitySettings) {
            ((AccessibilitySettings) fragment)
                    .setDelegate(new ChromeAccessibilitySettingsDelegate());
        }
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If SettingsActivity is exported, then it's vulnerable to a fragment injection
            // exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("SettingsActivity must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }

    /**
     * Set device status bar to match the activity background color, if supported.
     */
    private void setStatusBarColor() {
        // On P+, the status bar color is set via the XML theme.
        if ((!DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)
                    && VERSION.SDK_INT >= Build.VERSION_CODES.P)
                || (DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)
                        && !ChromeFeatureList.sTabStripRedesign.isEnabled()
                        && VERSION.SDK_INT >= Build.VERSION_CODES.P)) {
            return;
        }

        if (UiUtils.isSystemUiThemingDisabled()) return;

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        ApiCompatibilityUtils.setStatusBarIconColor(getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }
}
