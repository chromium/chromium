// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.BuildInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ApplicationLifetime;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsCoordinator;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragmentBasic;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsSettings;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.password_check.PasswordCheckComponentUiFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckFragmentView;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditUiFactory;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment;
import org.chromium.chrome.browser.privacy_sandbox.ChromeTrackingProtectionDelegate;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragmentBase;
import org.chromium.chrome.browser.safety_check.SafetyCheckCoordinator;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckUpdatesDelegateImpl;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockLauncher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.privacy_sandbox.TrackingProtectionSettings;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The Chrome settings activity.
 *
 * <p>This activity displays a single {@link Fragment}, typically a {@link
 * PreferenceFragmentCompat}. As the user navigates through settings, a separate Settings activity
 * is created for each screen. Thus each fragment may freely modify its activity's action bar or
 * title. This mimics the behavior of {@link android.preference.PreferenceActivity}.</p>
 */
public class SettingsActivity extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback,
                SnackbarManageable,
                DisplayStyleObserver {
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
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

    private ManagedBottomSheetController mBottomSheetController;

    private OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier =
            new OneshotSupplierImpl<>();

    @Nullable
    private UiConfig mUiConfig;

    private Profile mProfile;

    private @Nullable PaddedItemDecorationWithDivider mItemDecoration;
    private int mMinWidePaddingPixels;

    // This is only used on automotive.
    private @Nullable MissingDeviceLockLauncher mMissingDeviceLockLauncher;

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
        mProfile = Profile.getLastUsedRegularProfile();

        super.onCreate(savedInstanceState);

        setContentView(R.layout.settings_activity);

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        mIsNewlyCreated = savedInstanceState == null;
        mMinWidePaddingPixels =
                getResources().getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            if (initialFragment == null) initialFragment = MainSettings.class.getName();

            Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.content, fragment)
                    // Set width constraints after commit is done, since recycler view is not
                    // accessible before transaction completes.
                    .runOnCommit(this::configureWideDisplayStyle)
                    .commit();
        } else {
            // Still commit the wide screen configuration without replacing the fragment content.
            // Using FragmentTransaction so that the config is set after view is created, and before
            // fragment is shown.
            getSupportFragmentManager()
                    .beginTransaction()
                    .runOnCommit(this::configureWideDisplayStyle)
                    .commit();
        }

        setStatusBarColor();
        initBottomSheet();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Set width constraints.
        configureWideDisplayStyle();
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered
     * by adding padding to both sides.
     */
    private void configureWideDisplayStyle() {
        if (mUiConfig != null) {
            mUiConfig.updateDisplayStyle();
            return;
        }

        View content = findViewById(R.id.content);
        RecyclerView recyclerView = findViewById(R.id.recycler_view);
        // For settings with a recycler view, add paddings to the side so the content is
        // scrollable; otherwise, add the padding to the content.
        View paddedView = recyclerView == null ? content : recyclerView;
        mUiConfig = new UiConfig(paddedView);
        mUiConfig.addObserver(this);
        if (!hasPreferenceRecyclerView(recyclerView)) {
            ViewResizer.createAndAttach(paddedView, mUiConfig, 0, mMinWidePaddingPixels);
            return;
        }

        // Configure divider style if the fragment has a recycler view.
        // Remove the default divider that PreferenceFragmentCompat initialized. This is a
        // workaround as outer class has no access to the private DividerDecoration in
        // PreferenceFragmentCompat. See https://crbug.com/1293429.
        ((PreferenceFragmentCompat) getMainFragment()).setDivider(null);

        CustomDividerFragment customDividerFragment =
                getMainFragment() instanceof CustomDividerFragment
                        ? (CustomDividerFragment) getMainFragment()
                        : null;
        Supplier<Integer> itemOffsetSupplier =
                () -> getItemOffset(mUiConfig.getCurrentDisplayStyle());
        mItemDecoration = new PaddedItemDecorationWithDivider(itemOffsetSupplier);
        Drawable dividerDrawable = getDividerDrawable();
        // Early return if (a)Fragment implements CustomDividerFragment and explicitly don't
        // want a divider OR (b) dividerDrawable not defined.
        if ((customDividerFragment != null && !customDividerFragment.hasDivider())
                || dividerDrawable == null) {
            recyclerView.addItemDecoration(mItemDecoration);
            return;
        }
        // Configure the customized divider for the rest of the Fragments.
        Supplier<Integer> dividerStartPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerStartPadding()
                                : 0;
        Supplier<Integer> dividerEndPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerEndPadding()
                                : 0;
        mItemDecoration.setDividerWithPadding(
                dividerDrawable, dividerStartPaddingSupplier, dividerEndPaddingSupplier);
        recyclerView.addItemDecoration(mItemDecoration);
    }

    @NonNull
    private Integer getItemOffset(DisplayStyle displayStyle) {
        if (displayStyle.isWide()) {
            return ViewResizerUtil.computePaddingForWideDisplay(this, mMinWidePaddingPixels);
        }
        return 0;
    }

    private boolean hasPreferenceRecyclerView(RecyclerView recyclerView) {
        return recyclerView != null && (getMainFragment() instanceof PreferenceFragmentCompat);
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

        mBottomSheetController = BottomSheetControllerFactory.createBottomSheetController(
                () -> mScrim, (sheet) -> {}, getWindow(),
                KeyboardVisibilityDelegate.getInstance(), () -> sheetContainer);

        mBottomSheetControllerSupplier.set(mBottomSheetController);
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

        if (fragment instanceof BaseSiteSettingsFragment) {
            ChromeSiteSettingsDelegate delegate =
                    (ChromeSiteSettingsDelegate) (((BaseSiteSettingsFragment) fragment)
                                                          .getSiteSettingsDelegate());
            delegate.setSnackbarManager(mSnackbarManager);
        }
        if (fragment instanceof PrivacySandboxSettingsBaseFragment) {
            ((PrivacySandboxSettingsBaseFragment) fragment)
                    .setSnackbarManager(getSnackbarManager());
        }
        initBackPressHandler();
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

        checkForMissingDeviceLockOnAutomotive();
    }

    private void checkForMissingDeviceLockOnAutomotive() {
        if (BuildInfo.getInstance().isAutomotive) {
            if (mMissingDeviceLockLauncher == null) {
                mMissingDeviceLockLauncher = new MissingDeviceLockLauncher(
                        this, mProfile, getModalDialogManagerSupplier().get());
            }
            mMissingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock();
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
        help.setIcon(TraceEventVectorDrawableCompat.create(
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
            HelpAndFeedbackLauncherImpl.getForProfile(mProfile).show(
                    this, getString(R.string.help_context_settings), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void initBackPressHandler() {
        // Handlers registered last will be called first.
        registerMainFragmentBackPressHandler();
        if (ChromeFeatureList.sPrivacyGuidePostMVP.isEnabled()) {
            registerBottomSheetBackPressHandler();
        }
    }

    private void registerMainFragmentBackPressHandler() {
        Fragment activeFragment = getMainFragment();
        if (BackPressManager.isSecondaryActivityEnabled()) {
            if (activeFragment instanceof BackPressHandler) {
                BackPressHelper.create(activeFragment.getViewLifecycleOwner(),
                        getOnBackPressedDispatcher(), (BackPressHandler) activeFragment,
                        SecondaryActivity.SETTINGS);
            }
        } else if (activeFragment instanceof BackPressHelper.ObsoleteBackPressedHandler) {
            BackPressHelper.create(activeFragment.getViewLifecycleOwner(),
                    getOnBackPressedDispatcher(),
                    (BackPressHelper.ObsoleteBackPressedHandler) activeFragment,
                    SecondaryActivity.SETTINGS);
        }
    }

    private void registerBottomSheetBackPressHandler() {
        if (mBottomSheetController == null) return;

        BackPressHandler bottomSheetBackPressHandler =
                mBottomSheetController.getBottomSheetBackPressHandler();
        if (bottomSheetBackPressHandler != null) {
            if (BackPressManager.isSecondaryActivityEnabled()) {
                BackPressHelper.create(this, getOnBackPressedDispatcher(),
                        bottomSheetBackPressHandler, SecondaryActivity.SETTINGS);
            } else {
                BackPressHelper.create(this, getOnBackPressedDispatcher(),
                        mBottomSheetController::handleBackPress, SecondaryActivity.SETTINGS);
            }
        }
    }

    @Override
    public void onAttachFragment(Fragment fragment) {
        // Common dependencies attachments.
        if (fragment instanceof ProfileDependentSetting) {
            ((ProfileDependentSetting) fragment).setProfile(mProfile);
        }
        if (fragment instanceof FragmentSettingsLauncher) {
            FragmentSettingsLauncher fragmentSettingsLauncher = (FragmentSettingsLauncher) fragment;
            fragmentSettingsLauncher.setSettingsLauncher(mSettingsLauncher);
        }
        if (fragment instanceof FragmentHelpAndFeedbackLauncher) {
            FragmentHelpAndFeedbackLauncher fragmentHelpAndFeedbackLauncher =
                    (FragmentHelpAndFeedbackLauncher) fragment;
            fragmentHelpAndFeedbackLauncher.setHelpAndFeedbackLauncher(
                    HelpAndFeedbackLauncherImpl.getForProfile(mProfile));
        }

        // Settings screen specific attachments.
        if (fragment instanceof MainSettings) {
            ((MainSettings) fragment)
                    .setModalDialogManagerSupplier(getModalDialogManagerSupplier());
        }
        if (fragment instanceof BaseSiteSettingsFragment) {
            BaseSiteSettingsFragment baseSiteSettingsFragment =
                    ((BaseSiteSettingsFragment) fragment);
            baseSiteSettingsFragment.setSiteSettingsDelegate(
                    new ChromeSiteSettingsDelegate(this, mProfile));
            baseSiteSettingsFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof SafetyCheckSettingsFragment) {
            SafetyCheckCoordinator.create((SafetyCheckSettingsFragment) fragment,
                    new SafetyCheckUpdatesDelegateImpl(), mSettingsLauncher,
                    SyncConsentActivityLauncherImpl.get(), getModalDialogManagerSupplier(),
                    SyncServiceFactory.getForProfile(mProfile));
        }
        if (fragment instanceof PasswordCheckFragmentView) {
            PasswordCheckComponentUiFactory.create((PasswordCheckFragmentView) fragment,
                    HelpAndFeedbackLauncherImpl.getForProfile(mProfile), mSettingsLauncher,
                    LaunchIntentDispatcher::createCustomTabActivityIntent,
                    IntentUtils::addTrustedIntentExtras);
        }
        if (fragment instanceof CredentialEntryFragmentViewBase) {
            CredentialEditUiFactory.create((CredentialEntryFragmentViewBase) fragment,
                    HelpAndFeedbackLauncherImpl.getForProfile(mProfile));
        }
        if (fragment instanceof SearchEngineSettings) {
            SearchEngineSettings settings = (SearchEngineSettings) fragment;
            settings.setDisableAutoSwitchRunnable(
                    () -> LocaleManager.getInstance().setSearchEngineAutoSwitch(false));
            settings.setSettingsLauncher(mSettingsLauncher);
        }
        if (fragment instanceof ImageDescriptionsSettings) {
            ImageDescriptionsSettings imageFragment = (ImageDescriptionsSettings) fragment;
            Bundle extras = imageFragment.getArguments();
            if (extras != null) {
                extras.putBoolean(ImageDescriptionsSettings.IMAGE_DESCRIPTIONS,
                        ImageDescriptionsController.getInstance().imageDescriptionsEnabled(
                                mProfile));
                extras.putBoolean(ImageDescriptionsSettings.IMAGE_DESCRIPTIONS_DATA_POLICY,
                        ImageDescriptionsController.getInstance().onlyOnWifiEnabled(mProfile));
            }
            imageFragment.setDelegate(ImageDescriptionsController.getInstance().getDelegate());
        }
        if (fragment instanceof PrivacySandboxSettingsBaseFragment) {
            PrivacySandboxSettingsBaseFragment sandboxFragment =
                    (PrivacySandboxSettingsBaseFragment) fragment;
            sandboxFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
            sandboxFragment.setCookieSettingsIntentHelper((Context context) -> {
                SiteSettingsHelper.showCategorySettings(
                        context, mProfile, SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
            });
        }
        if (fragment instanceof SafeBrowsingSettingsFragmentBase) {
            SafeBrowsingSettingsFragmentBase safeBrowsingFragment =
                    (SafeBrowsingSettingsFragmentBase) fragment;
            safeBrowsingFragment.setCustomTabIntentHelper(
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
            PrivacyGuideFragment pgFragment = (PrivacyGuideFragment) fragment;
            pgFragment.setBottomSheetControllerSupplier(mBottomSheetControllerSupplier);
            pgFragment.setCustomTabIntentHelper(
                    LaunchIntentDispatcher::createCustomTabActivityIntent);
        }
        if (fragment instanceof AccessibilitySettings) {
            ((AccessibilitySettings) fragment)
                    .setDelegate(new ChromeAccessibilitySettingsDelegate(mProfile));
        }
        if (fragment instanceof PasswordSettings) {
            ((PasswordSettings) fragment).setBottomSheetController(mBottomSheetController);
        }
        if (fragment instanceof AutofillOptionsFragment) {
            AutofillOptionsCoordinator.createFor((AutofillOptionsFragment) fragment);
        }
        if (fragment instanceof TrackingProtectionSettings) {
            TrackingProtectionSettings tpFragment = ((TrackingProtectionSettings) fragment);
            tpFragment.setTrackingProtectionDelegate(
                    new ChromeTrackingProtectionDelegate(mProfile));
            tpFragment.setCustomTabIntentHelper(
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

    /**
     * Set device status bar to match the activity background color, if supported.
     */
    private void setStatusBarColor() {
        // On P+, the status bar color is set via the XML theme.
        if (VERSION.SDK_INT >= Build.VERSION_CODES.P && !BuildInfo.getInstance().isAutomotive
                && (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)
                        || (DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)
                                && !ChromeFeatureList.sTabStripRedesign.isEnabled()))) {
            return;
        }

        if (UiUtils.isSystemUiThemingDisabled()) return;

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        UiUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        UiUtils.setStatusBarIconColor(getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    // Get the divider drawable from AndroidX Pref attribute to keep things consistent.
    private Drawable getDividerDrawable() {
        TypedArray ta = obtainStyledAttributes(null, R.styleable.PreferenceFragmentCompat,
                R.attr.preferenceFragmentCompatStyle, 0);
        final Drawable divider =
                ta.getDrawable(R.styleable.PreferenceFragmentCompat_android_divider);
        ta.recycle();

        return divider;
    }

    @Override
    public void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        RecyclerView recyclerView = findViewById(R.id.recycler_view);
        if (hasPreferenceRecyclerView(recyclerView)) {
            // Invalidate decorations to reset.
            recyclerView.invalidateItemDecorations();
        }
    }
}
