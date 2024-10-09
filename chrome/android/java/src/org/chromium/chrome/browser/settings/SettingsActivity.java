// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Color;
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
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockLauncher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.Locale;

/**
 * The Chrome settings activity.
 *
 * <p>This activity displays a single {@link Fragment}, typically a {@link
 * PreferenceFragmentCompat}. There are two types of fragments shown in the activity:
 * <i>embeddable</i> fragments that implement {@link EmbeddableSettingsPage}, and <i>standalone</i>
 * fragments that do not implement it. Embeddable fragments may be embedded into a column in the
 * multi-column settings UI, if it is enabled and the window is large enough. Standalone fragments,
 * in contrast, are always shown as occupying the whole window.
 *
 * <p>Embeddable fragments must not modify the activity UI outside of the fragment, e.g. the
 * activity title and the action bar, because the same activity instance is shared among multiple
 * fragments as the user navigates through the settings. Instead, fragments should implement methods
 * in {@link EmbeddableSettingsPage} to ask the activity to update its UI appropriately.
 *
 * <p>Standalone fragments may modify the activity UI as needed. A standalone fragment is always
 * launched with a fresh settings activity instance that is not shared with other fragments.
 */
public class SettingsActivity extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback, SnackbarManageable {
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";

    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    static final String EXTRA_SHOW_FRAGMENT_STANDALONE = "show_fragment_standalone";

    /** The current instance of SettingsActivity in the resumed state, if any. */
    private static SettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    private boolean mStandalone;
    private Profile mProfile;
    private ScrimCoordinator mScrim;
    private ManagedBottomSheetController mManagedBottomSheetController;
    private final OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier =
            new OneshotSupplierImpl<>();

    private final OneshotSupplierImpl<SnackbarManager> mSnackbarManagerSupplier =
            new OneshotSupplierImpl<>();

    // An intent that was received in onNewIntent and would cause fragment transactions, but is
    // pending for processing in the next onResume call. See onNewIntent for why we can not directly
    // process those intents in onNewIntent.
    private Intent mPendingNewIntent;

    // This is only used on automotive.
    private @Nullable MissingDeviceLockLauncher mMissingDeviceLockLauncher;

    private static final String MAIN_FRAGMENT_TAG = "settings_main";

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mStandalone = getIntent().getBooleanExtra(EXTRA_SHOW_FRAGMENT_STANDALONE, false);

        setTitle(R.string.settings);
        ensureActivityNotExported();

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Settings after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        mProfile = ProfileManager.getLastUsedRegularProfile();

        // Register fragment lifecycle callbacks before calling super.onCreate() because it may
        // create fragments if there is a saved instance state.
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.registerFragmentLifecycleCallbacks(
                new FragmentDependencyProvider(
                        this,
                        mProfile,
                        mSnackbarManagerSupplier,
                        mBottomSheetControllerSupplier,
                        getModalDialogManagerSupplier()),
                true /* recursive */);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new WideDisplayPaddingApplier(), false /* recursive */);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new SettingsMetricsReporter(), false /* recursive */);
        if (!mStandalone) {
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new TitleUpdater(), false /* recursive */);
        }

        super.onCreate(savedInstanceState);

        setContentView(R.layout.settings_activity);

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        mIsNewlyCreated = savedInstanceState == null;

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            Fragment fragment = instantiateMainFragment(getIntent());
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.content, fragment, MAIN_FRAGMENT_TAG)
                    .setCustomAnimations(
                            R.anim.shared_x_axis_open_enter,
                            R.anim.shared_x_axis_open_exit,
                            R.anim.shared_x_axis_close_enter,
                            R.anim.shared_x_axis_close_exit)
                    .commit();
        }

        setStatusBarColor();
        initBottomSheet();

        mSnackbarManagerSupplier.set(
                new SnackbarManager(this, findViewById(android.R.id.content), null));
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        // This callback is called only when the settings UI is operating in the single activity
        // mode.
        assert ChromeFeatureList.sSettingsSingleActivity.isEnabled();

        if (mStandalone) {
            // A standalone activity attempted to launch a non-standalone activity, but the intent
            // was delivered to the standalone activity itself because of FLAG_ACTIVITY_SINGLE_TOP.
            // Resend the intent without the flag to start a new activity. Bouncing activities has
            // some cost in terms of time to launch the final activity, but this is fairly a rare
            // flow anyway.
            intent.removeFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
            return;
        }

        // Android system briefly pauses an activity before calling its onNewIntent, then resume it
        // soon. We defer making a fragment transaction to onResume because doing it here breaks
        // fragment animations as all pending animations are cleared when an activity is resumed.
        assert mPendingNewIntent == null;
        mPendingNewIntent = intent;
    }

    private Fragment instantiateMainFragment(Intent intent) {
        String fragmentName = intent.getStringExtra(EXTRA_SHOW_FRAGMENT);
        if (fragmentName == null) {
            fragmentName = MainSettings.class.getName();
        }
        Bundle arguments = intent.getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        return Fragment.instantiate(this, fragmentName, arguments);
    }

    /** Set up the bottom sheet for this activity. */
    private void initBottomSheet() {
        ViewGroup sheetContainer = findViewById(R.id.sheet_container);
        mScrim =
                new ScrimCoordinator(
                        this,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {
                                // TODO: Implement if status bar needs to change color with the
                                // scrim.
                            }

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {
                                // TODO: Implement if navigation bar needs to change color with the
                                // scrim.
                            }
                        },
                        (ViewGroup) sheetContainer.getParent(),
                        getColor(R.color.default_scrim_color));

        mManagedBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrim,
                        CallbackUtils.emptyCallback(),
                        getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0,
                        /* desktopWindowStateProvider= */ null);
        mBottomSheetControllerSupplier.set(mManagedBottomSheetController);
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        startSettings(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new settings showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startSettings(@Nullable String fragmentClass, @Nullable Bundle args) {
        Intent intent = SettingsIntentUtil.createIntent(this, fragmentClass, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        initBackPressHandler();
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of SettingsActivity at the same
        // time (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null
                && sResumedInstance.getTaskId() != getTaskId()
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

        // If there is a pending intent to process from onNewIntent, process it now.
        if (mPendingNewIntent != null) {
            Fragment fragment = instantiateMainFragment(mPendingNewIntent);
            mPendingNewIntent = null;
            getSupportFragmentManager()
                    .beginTransaction()
                    .setReorderingAllowed(true)
                    .setCustomAnimations(
                            R.anim.shared_x_axis_open_enter,
                            R.anim.shared_x_axis_open_exit,
                            R.anim.shared_x_axis_close_enter,
                            R.anim.shared_x_axis_close_exit)
                    .replace(R.id.content, fragment, MAIN_FRAGMENT_TAG)
                    .addToBackStack(null)
                    .commit();
        }
    }

    private void checkForMissingDeviceLockOnAutomotive() {
        if (BuildInfo.getInstance().isAutomotive) {
            if (mMissingDeviceLockLauncher == null) {
                mMissingDeviceLockLauncher =
                        new MissingDeviceLockLauncher(
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

    @Override
    protected void onDestroy() {
        mScrim.destroy();
        super.onDestroy();
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
        MenuItem help =
                menu.add(
                        Menu.NONE,
                        R.id.menu_id_general_help,
                        Menu.CATEGORY_SECONDARY,
                        R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
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
            finishCurrentSettings(mainFragment);
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                    .show(this, getString(R.string.help_context_settings), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void initBackPressHandler() {
        // Handlers registered last will be called first.
        registerMainFragmentBackPressHandler();
        registerBottomSheetBackPressHandler();
    }

    private void registerMainFragmentBackPressHandler() {
        Fragment activeFragment = getMainFragment();
        if (activeFragment instanceof BackPressHandler) {
            // We do not support embeddable fragments to implement BackPressHandler as it requires
            // keeping track of the main fragment while there is no real use case for it.
            assert !ChromeFeatureList.sSettingsSingleActivity.isEnabled() || mStandalone;
            BackPressHelper.create(
                    activeFragment.getViewLifecycleOwner(),
                    getOnBackPressedDispatcher(),
                    (BackPressHandler) activeFragment,
                    SecondaryActivity.SETTINGS);
        }
    }

    private void registerBottomSheetBackPressHandler() {
        BackPressHelper.create(
                this,
                getOnBackPressedDispatcher(),
                mBottomSheetControllerSupplier.get().getBottomSheetBackPressHandler(),
                SecondaryActivity.SETTINGS);
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManagerSupplier.get();
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

    /** Set device status bar to match the activity background color, if supported. */
    private void setStatusBarColor() {
        // On P+, the status bar color is set via the XML theme.
        if (VERSION.SDK_INT >= Build.VERSION_CODES.P
                && !BuildInfo.getInstance().isAutomotive
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)) {
            return;
        }

        if (UiUtils.isSystemUiThemingDisabled()) return;

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        UiUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        UiUtils.setStatusBarIconColor(
                getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    /**
     * Finishes the current fragment.
     *
     * <p>This method asks the activity to show the previous fragment. If the back stack is empty,
     * the activity itself is finished.
     *
     * <p>If the given fragment is not the current one, this method does nothing.
     *
     * <p>This method is package-private because it is used by {@link SettingsNavigationImpl}. Use
     * {@link SettingsNavigation} to call this method from fragments, instead of calling it
     * directly.
     *
     * @param fragment The expected current fragment.
     */
    @SuppressLint("ReferenceEquality")
    void finishCurrentSettings(Fragment fragment) {
        if (getMainFragment() != fragment) {
            return;
        }

        if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
            FragmentManager fragmentManager = getSupportFragmentManager();
            if (fragmentManager.getBackStackEntryCount() == 0) {
                finish();
            } else {
                // Execute the pop operation immediately to avoid popping the back stack N times
                // when the current fragment calls this method N times in a row.
                fragmentManager.popBackStackImmediate();
            }
        } else {
            finish();
        }
    }

    private class TitleUpdater extends FragmentManager.FragmentLifecycleCallbacks {
        private final Callback<String> mSetTitleCallback =
                (title) -> {
                    if (title == null) {
                        title = "";
                    }
                    setTitle(title);
                };

        private ObservableSupplier<String> mCurrentPageTitle;

        @Override
        public void onFragmentResumed(
                @NonNull FragmentManager fragmentManager, @NonNull Fragment fragment) {
            if (!MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            // TitleUpdater is enabled only when the fragment implements EmbeddableSettingsPage.
            EmbeddableSettingsPage settingsFragment = (EmbeddableSettingsPage) fragment;

            if (mCurrentPageTitle != null) {
                mCurrentPageTitle.removeObserver(mSetTitleCallback);
            }
            mCurrentPageTitle = settingsFragment.getPageTitle();
            mCurrentPageTitle.addObserver(mSetTitleCallback);
        }
    }

    private class WideDisplayPaddingApplier extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                @NonNull FragmentManager fragmentManager,
                @NonNull Fragment fragment,
                @NonNull View view,
                @Nullable Bundle savedInstanceState) {
            if (MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                // Apply the wide display style after the main fragment is committed since its views
                // (particularly a recycler view) are not accessible before the transaction
                // completes.
                WideDisplayPadding.apply(fragment, SettingsActivity.this);
            }
        }
    }

    private static class SettingsMetricsReporter
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentAttached(
                @NonNull FragmentManager fragmentManager,
                @NonNull Fragment fragment,
                @NonNull Context context) {
            if (!MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            String className = fragment.getClass().getSimpleName();
            RecordHistogram.recordSparseHistogram(
                    "Settings.FragmentAttached", className.hashCode());
            // Log hashCode to easily add new class names to enums.xml.
            Log.d(
                    "SettingsActivity",
                    String.format(
                            Locale.ENGLISH,
                            "Settings.FragmentAttached: <int value=\"%d\" label=\"%s\"/>",
                            className.hashCode(),
                            className));
        }
    }
}
