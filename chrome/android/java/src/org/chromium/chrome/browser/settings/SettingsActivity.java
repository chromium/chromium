// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;

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
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentBasic;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUiFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckFragmentView;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditUiFactory;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase;
import org.chromium.chrome.browser.privacy_sandbox.FlocSettingsFragment;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragment;
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
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.ui.UiUtils;

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

        Resources res = getResources();

        setTaskDescription(new ActivityManager.TaskDescription(res.getString(R.string.app_name),
                BitmapFactory.decodeResource(res, R.mipmap.app_icon),
                ApiCompatibilityUtils.getColor(res, R.color.default_primary_color)));

        setStatusBarColor();
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
        super.onCreateOptionsMenu(menu);
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_general_help, Menu.CATEGORY_SECONDARY, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getTheme()));
        return true;
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

    @Override
    public void onBackPressed() {
        Fragment activeFragment = getMainFragment();
        if (!(activeFragment instanceof OnBackPressedListener)) {
            super.onBackPressed();
            return;
        }
        OnBackPressedListener listener = (OnBackPressedListener) activeFragment;
        if (!listener.onBackPressed()) {
            // Fragment hasn't handled this event, fall back to AppCompatActivity handling.
            super.onBackPressed();
        }
    }

    @Override
    public void onAttachFragment(Fragment fragment) {
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
                    new SafetyCheckUpdatesDelegateImpl(this), mSettingsLauncher,
                    SyncConsentActivityLauncherImpl.get());
        }
        if (fragment instanceof PasswordCheckFragmentView) {
            PasswordCheckComponentUiFactory.create((PasswordCheckFragmentView) fragment,
                    HelpAndFeedbackLauncherImpl.getInstance(), mSettingsLauncher,
                    LaunchIntentDispatcher::createCustomTabActivityIntent,
                    IntentUtils::addTrustedIntentExtras);
        } else if (fragment instanceof PasswordCheckEditFragmentView) {
            PasswordCheckEditFragmentView editFragment = (PasswordCheckEditFragmentView) fragment;
            editFragment.setCheckProvider(
                    () -> PasswordCheckFactory.getOrCreate(mSettingsLauncher));
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
        if (fragment instanceof PrivacySandboxSettingsFragment) {
            ((PrivacySandboxSettingsFragment) fragment)
                    .setCustomTabIntentHelper(
                            LaunchIntentDispatcher::createCustomTabActivityIntent);
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

    @Override
    protected void applyThemeOverlays() {
        super.applyThemeOverlays();

        setTheme(R.style.ThemeRefactorOverlay_Disabled_Settings);
    }

    /**
     * Set device status bar to match the activity background color, if supported.
     */
    private void setStatusBarColor() {
        // On P+, the status bar color is set via the XML theme.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) return;

        if (UiUtils.isSystemUiThemingDisabled()) return;

        // Dark status icons only supported on M+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        ApiCompatibilityUtils.setStatusBarIconColor(getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));
    }
}
