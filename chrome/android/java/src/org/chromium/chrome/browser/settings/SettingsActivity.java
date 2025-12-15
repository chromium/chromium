// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.window.layout.WindowMetricsCalculator;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.BackPressHelper.OnKeyDownHandler;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockLauncher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

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
@NullMarked
public class SettingsActivity extends ChromeBaseAppCompatActivity
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback,
                SnackbarManageable,
                PreferenceUpdateObserver {
    private static final String TAG = "SettingsActivity";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";

    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    static final String EXTRA_SHOW_FRAGMENT_STANDALONE = "show_fragment_standalone";
    static final String EXTRA_ADD_TO_BACK_STACK = "add_to_back_stack";

    /** The current instance of SettingsActivity in the resumed state, if any. */
    private static @Nullable SettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    private boolean mStandalone;
    private Profile mProfile;
    private ScrimManager mScrimManager;
    private ManagedBottomSheetController mManagedBottomSheetController;
    private final OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier =
            new OneshotSupplierImpl<>();

    private final OneshotSupplierImpl<SnackbarManager> mSnackbarManagerSupplier =
            new OneshotSupplierImpl<>();

    // Number of popback requested after the fragment manager saved its state.
    private int mPendingPopBackCount;

    // An intent that was received in onNewIntent and would cause fragment transactions, but is
    // pending for processing in the next onResume call. See onNewIntent for why we can not directly
    // process those intents in onNewIntent.
    private @Nullable Intent mPendingNewIntent;

    // Used to avoid finishing the same fragment multiple times. If the referent is identical to the
    // result of getMainFragment(), it should be considered already finished. Otherwise it should be
    // ignored.
    private @Nullable WeakReference<Fragment> mFinishedMainFragment;

    // This is only used on automotive.
    private @Nullable MissingDeviceLockLauncher mMissingDeviceLockLauncher;

    // Refers the instance only when SettingsMultiColumn is enabled.
    private @Nullable MultiColumnSettings mMultiColumnSettings;

    // Used to manage and show new intents;
    private IntentRequestTracker mIntentRequestTracker;

    private static final String MAIN_FRAGMENT_TAG = "settings_main";
    public static final String MULTI_COLUMN_FRAGMENT_TAG = "multi_column_settings";

    private final Map<PreferenceFragmentCompat, ContainmentItemDecoration> mItemDecorations =
            new HashMap<>();
    private final Map<PreferenceFragmentCompat, ViewTreeObserver.OnGlobalLayoutListener>
            mGlobalLayoutListeners = new HashMap<>();

    private @Nullable SettingsSearchCoordinator mSearchCoordinator;

    private @Nullable OnKeyDownHandler mMainFragmentKeyDownHandler;
    private @Nullable OnKeyDownHandler mBottomSheetKeyDownHandler;

    // Update handler of the Settings activity title. mTitleUpdater is used (i.e. nonnull)
    // in multi-column mode is disabled, and mMultiColumnTitleUpdater is used iff
    // multi-column mode is enabled.
    private @Nullable TitleUpdater mTitleUpdater;
    private @Nullable MultiColumnTitleUpdater mMultiColumnTitleUpdater;

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
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
                /* recursive= */ true);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new WideDisplayPaddingApplier(), /* recursive= */ true);
        fragmentManager.registerFragmentLifecycleCallbacks(
                new SettingsMetricsReporter(), /* recursive= */ true);

        if (isContainmentEnabled()) {
            // In multi-column mode, the main settings fragment is a child of the
            // MultiColumnSettings fragment, so the callbacks must be registered recursively.
            boolean recursive = true;
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new FragmentManager.FragmentLifecycleCallbacks() {
                        @Override
                        public void onFragmentAttached(
                                @NonNull FragmentManager fm,
                                @NonNull Fragment f,
                                @NonNull Context context) {
                            if (f instanceof PreferenceUpdateObserver.Provider provider) {
                                provider.setPreferenceUpdateObserver(SettingsActivity.this);
                            }
                        }

                        @Override
                        public void onFragmentDetached(
                                @NonNull FragmentManager fm, @NonNull Fragment f) {
                            if (f instanceof PreferenceUpdateObserver.Provider provider) {
                                provider.removePreferenceUpdateObserver();
                            }
                        }

                        @Override
                        public void onFragmentViewCreated(
                                @NonNull FragmentManager fm,
                                @NonNull Fragment fragment,
                                @NonNull View v,
                                @Nullable Bundle savedInstanceState) {
                            if (!(fragment
                                    instanceof PreferenceFragmentCompat preferenceFragmentCompat))
                                return;
                            postUpdateContainmentOnLayout(preferenceFragmentCompat);
                        }

                        @Override
                        public void onFragmentViewDestroyed(
                                @NonNull FragmentManager fm, @NonNull Fragment f) {
                            if (f instanceof PreferenceFragmentCompat preferenceFragmentCompat) {
                                mItemDecorations.remove(preferenceFragmentCompat);
                                ViewTreeObserver.OnGlobalLayoutListener listener =
                                        mGlobalLayoutListeners.remove(preferenceFragmentCompat);
                                if (listener != null
                                        && preferenceFragmentCompat.getView() != null) {
                                    preferenceFragmentCompat
                                            .getView()
                                            .getViewTreeObserver()
                                            .removeOnGlobalLayoutListener(listener);
                                }
                            }
                        }
                    },
                    recursive);
        }

        super.onCreate(savedInstanceState);

        setContentView(R.layout.settings_activity);

        Toolbar actionBar = findViewById(R.id.action_bar);
        setSupportActionBar(actionBar);
        assumeNonNull(getSupportActionBar()).setDisplayHomeAsUpEnabled(true);

        mIsNewlyCreated = savedInstanceState == null;

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            // In standalone mode, we shouldn't have multi column.
            if (!mStandalone && isMultiColumnSettingEnabled()) {
                // Do NOT set MAIN_FRAGMENT_TAG in this case, so page-title updating,
                // setting the padding depending on window size, and metrics are temporarily
                // disabled for development.
                // TODO(crbug.com/404074032): Implement them back.
                var transaction = fragmentManager.beginTransaction();
                mMultiColumnSettings = new MultiColumnSettings();
                mMultiColumnSettings.setPendingFragmentIntent(getIntent());
                transaction.replace(R.id.content, mMultiColumnSettings, MULTI_COLUMN_FRAGMENT_TAG);
                transaction.commit();
            } else {
                Fragment fragment = instantiateMainFragment(getIntent());
                var transaction = fragmentManager.beginTransaction();
                transaction.replace(R.id.content, fragment, MAIN_FRAGMENT_TAG);
                setFragmentAnimation(transaction, fragment);
                transaction.commit();
            }
        } else {
            mMultiColumnSettings =
                    (MultiColumnSettings)
                            fragmentManager.findFragmentByTag(MULTI_COLUMN_FRAGMENT_TAG);
        }

        if (!mStandalone) {
            if (isMultiColumnSettingEnabled()) {
                assert mMultiColumnSettings != null;
                createMultiColumnTitleUpdater();
            } else {
                mTitleUpdater = new TitleUpdater();
                fragmentManager.registerFragmentLifecycleCallbacks(
                        mTitleUpdater, /* recursive= */ true);
            }
        }

        setStatusBarColor();
        initBottomSheet();

        mSnackbarManagerSupplier.set(new SnackbarManager(this, getContentView(), null));

        mIntentRequestTracker = IntentRequestTracker.createFromActivity(this);
        if (isContainmentEnabled()) {
            int backgroundColor = SemanticColorUtils.getSettingsBackgroundColor(this);
            findViewById(R.id.content).setBackgroundColor(backgroundColor);
            findViewById(R.id.app_bar_layout).setBackgroundColor(backgroundColor);
        }
        if (isContainmentEnabled() || isMultiColumnSettingEnabled()) {
            AppBarLayout appBarLayout = findViewById(R.id.app_bar_layout);
            appBarLayout.setElevation(0);
            appBarLayout.setStateListAnimator(null);
        }
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mMultiColumnSettings != null) {
            for (Fragment fragment :
                    mMultiColumnSettings.getChildFragmentManager().getFragments()) {
                if (fragment.isAdded()
                        && fragment instanceof PreferenceFragmentCompat preferenceFragmentCompat) {
                    postUpdateContainmentOnLayout(preferenceFragmentCompat);
                }
            }
        }
        if (mSearchCoordinator != null) mSearchCoordinator.onConfigurationChanged(newConfig);
    }

    // Helper method to post containment update on layout completion.
    private void postUpdateContainmentOnLayout(PreferenceFragmentCompat fragment) {
        if (fragment.getView() == null) return;

        // If there's an existing listener, remove it to avoid multiple triggers.
        if (mGlobalLayoutListeners.containsKey(fragment)) {
            fragment.getView()
                    .getViewTreeObserver()
                    .removeOnGlobalLayoutListener(mGlobalLayoutListeners.get(fragment));
        }

        ViewTreeObserver.OnGlobalLayoutListener listener =
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        if (fragment.getView() == null) return;
                        fragment.getView().getViewTreeObserver().removeOnGlobalLayoutListener(this);
                        mGlobalLayoutListeners.remove(fragment);
                        updateFragmentContainment(fragment);
                    }
                };
        fragment.getView().getViewTreeObserver().addOnGlobalLayoutListener(listener);
        mGlobalLayoutListeners.put(fragment, listener);
    }

    @Override
    protected boolean applyOverrides(Context baseContext, Configuration overrideConfig) {
        boolean result = super.applyOverrides(baseContext, overrideConfig);
        if (!UiAndroidFeatureList.sRefactorMinWidthContextOverride.isEnabled()) {

            // We override the smallestScreenWidthDp here to ensure mIsTablet which relies on
            // smallestScreenWidthDp is set based on display size instead of window size.
            overrideConfig.smallestScreenWidthDp =
                    DisplayUtil.getCurrentSmallestScreenWidth(baseContext);
            return true;
        }
        return result;
    }

    @RequiresNonNull("mMultiColumnSettings")
    private void createMultiColumnTitleUpdater() {
        if (!ChromeFeatureList.sSearchInSettings.isEnabled()) {
            createMultiColumTitleUpdaterInternal(findViewById(R.id.settings_detailed_pane_title));
        } else {
            getSupportFragmentManager()
                    .registerFragmentLifecycleCallbacks(
                            new FragmentManager.FragmentLifecycleCallbacks() {

                                @Override
                                public void onFragmentViewCreated(
                                        @NonNull FragmentManager fm,
                                        @NonNull Fragment f,
                                        @NonNull View v,
                                        @Nullable Bundle savedInstanceState) {
                                    assert mMultiColumnSettings != null;
                                    createMultiColumTitleUpdaterInternal(
                                            v.findViewById(R.id.settings_title_in_detailed_pane));
                                    fm.unregisterFragmentLifecycleCallbacks(this);
                                    createSearchCoordinator();
                                }
                            },
                            false);
        }
    }

    @RequiresNonNull("mMultiColumnSettings")
    private void createMultiColumTitleUpdaterInternal(LinearLayout titleContainer) {
        mMultiColumnTitleUpdater =
                new MultiColumnTitleUpdater(
                        mMultiColumnSettings,
                        titleContainer.getContext(),
                        titleContainer,
                        this::setTitle,
                        this::onTitleTapped);
        mMultiColumnSettings.addObserver(mMultiColumnTitleUpdater);
    }

    private void createSearchCoordinator() {
        assert ChromeFeatureList.sSearchInSettings.isEnabled();
        Callback<Integer> updateFirstVisibleTitle =
                isMultiColumnSettingEnabled()
                        ? assumeNonNull(mMultiColumnTitleUpdater)::setFirstVisibleTitleIndex
                        : CallbackUtils.emptyCallback();
        // TODO(crbug.com/439911511): Refactor to use `isTwoColumnSettingsVisible` instead of
        // `getUseMultiColumn`.
        // Ensure `mMultiColumnSettings.isTwoColumn()` is reliable by confirming layout pass
        // completion
        // before direct replacement, as premature usage can lead to incorrect behavior.
        mSearchCoordinator =
                new SettingsSearchCoordinator(
                        this,
                        this::getUseMultiColumn,
                        mMultiColumnSettings,
                        mItemDecorations,
                        mProfile,
                        updateFirstVisibleTitle);
        mSearchCoordinator.initializeSearchUi();
    }

    private void onTitleTapped(@Nullable String entryName) {
        if (mSearchCoordinator != null) mSearchCoordinator.onTitleTapped(entryName);
    }

    /**
     * Returns true if multi-column mode will be displayed. This happens when the flag
     * #settings-multicolumn is enabled and the screen width is broad enough to activate the
     * multi-column mode.
     */
    private boolean getUseMultiColumn() {
        if (!isMultiColumnSettingEnabled()) return false;

        var windowMetrics = WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(this);
        return windowMetrics.getBounds().width()
                >= getResources()
                        .getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width);
    }

    /** Returns true if the AndroidSettingsContainment feature is enabled. */
    private static boolean isContainmentEnabled() {
        return ChromeFeatureList.sAndroidSettingsContainment.isEnabled();
    }

    /** Returns true if the AndroidSettingsContainment feature is enabled. */
    private static boolean isMultiColumnSettingEnabled() {
        return ChromeFeatureList.sSettingsMultiColumn.isEnabled();
    }

    @Override
    public void onPreferencesUpdated(PreferenceFragmentCompat fragment) {
        postUpdateContainmentOnLayout(fragment);
    }

    /**
     * Applies or removes containment styling for fragments within the multi-column settings layout
     * based on whether the multi-column layout is currently active.
     */
    private void updateFragmentContainment(PreferenceFragmentCompat fragment) {
        if (!isContainmentEnabled() || fragment == null) {
            return;
        }

        if (isTwoColumnSettingsVisible() && fragment instanceof MainSettings mainSettingsFragment) {
            applyMainSettingsFragmentDecoration(mainSettingsFragment);
        } else {
            applyContainmentForFragment(fragment);
        }
    }

    /** Returns true if two-column mode is visible. */
    public boolean isTwoColumnSettingsVisible() {
        return isMultiColumnSettingEnabled()
                && mMultiColumnSettings != null
                && mMultiColumnSettings.isTwoColumn();
    }

    /**
     * Applies containment styling to the given fragment if containment is enabled and the fragment
     * is a valid {@link PreferenceFragmentCompat} with a list view.
     *
     * @param fragment The fragment to apply the styling to.
     */
    private void applyContainmentForFragment(PreferenceFragmentCompat fragment) {
        // Disable selection highlight of MainSettings in single-column layout
        if (fragment instanceof MainSettings mainSettings) {
            mainSettings.setMultiColumnSettings(null, null);
        }

        fragment.requireContext()
                .getTheme()
                .applyStyle(R.style.ThemeOverlay_Chromium_Settings_Containment, true);

        final var recyclerView = fragment.getListView();
        if (recyclerView == null) return;

        ContainmentItemController controller = new ContainmentItemController(SettingsActivity.this);
        ContainmentItemDecoration itemDecoration = mItemDecorations.get(fragment);
        if (itemDecoration == null) {
            itemDecoration = new ContainmentItemDecoration(controller);
            mItemDecorations.put(fragment, itemDecoration);
            recyclerView.addItemDecoration(itemDecoration);
        }
        itemDecoration.updatePreferenceStyles(
                controller.generatePreferenceStyles(
                        SettingsUtils.getVisiblePreferences(fragment.getPreferenceScreen())));
        recyclerView.invalidateItemDecorations();

        // Force a re-inflation of all views to ensure they pick up the new
        // theme.
        reInflateViews(fragment);
    }

    private void applyMainSettingsFragmentDecoration(MainSettings mainSettings) {
        int verticalMargin =
                getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_container_vertical_margin);
        int leftMargin = getResources().getDimensionPixelSize(R.dimen.settings_item_margin);
        float radius =
                getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        int selectedBackgroundColor =
                SemanticColorUtils.getSettingsMainMenuSelectedBackgroundColor(
                        mainSettings.requireContext());
        // TODO(crbug.com/439911511): `SelectionDecoration`'s name does not fully capture its
        // current responsibility, which inadvertently includes handling decoration removal
        // for `MainSettings` when in two-column mode. Consider renaming it to reflect this broader
        // role.
        mainSettings.setMultiColumnSettings(
                mMultiColumnSettings,
                new SelectionDecoration(
                        verticalMargin, leftMargin, radius, selectedBackgroundColor));
    }

    private void reInflateViews(PreferenceFragmentCompat fragment) {
        if (fragment.getListView() == null) return;

        var adapter = fragment.getListView().getAdapter();
        fragment.getListView().setAdapter(null);
        fragment.getListView().setAdapter(adapter);
    }

    @Override
    public void applyThemeOverlays() {
        if (isContainmentEnabled()) {
            applySingleThemeOverlay(R.style.ThemeOverlay_Chromium_Settings_Containment);
        }
        super.applyThemeOverlays();
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
        if (mMultiColumnSettings != null) {
            mMultiColumnSettings.setPendingFragmentIntent(intent);
        } else {
            mPendingNewIntent = intent;
        }
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
        // TODO: Observe scrim changes if status bar needs to change color with the scrim.
        mScrimManager =
                new ScrimManager(
                        this,
                        (ViewGroup) sheetContainer.getParent(),
                        ScrimClient.SETTINGS_ACTIVITY);

        mManagedBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrimManager,
                        CallbackUtils.emptyCallback(),
                        getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0,
                        /* desktopWindowStateManager= */ null);
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

        if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
            if (mPendingPopBackCount > 0) {
                RecordHistogram.recordCount100Histogram(
                        "Android.Settings.PendingPopBackWorked", mPendingPopBackCount);
                FragmentManager fragmentManager =
                        mMultiColumnSettings == null
                                ? getSupportFragmentManager()
                                : mMultiColumnSettings.getChildFragmentManager();
                if (fragmentManager.getBackStackEntryCount() <= mPendingPopBackCount) {
                    finish();
                } else {
                    var entry =
                            fragmentManager.getBackStackEntryAt(
                                    fragmentManager.getBackStackEntryCount()
                                            - mPendingPopBackCount);
                    fragmentManager.popBackStack(
                            entry.getId(), FragmentManager.POP_BACK_STACK_INCLUSIVE);
                }
                mPendingPopBackCount = 0;
            }
        }
        // If there is a pending intent to process from onNewIntent, process it now.
        if (mPendingNewIntent != null) {
            // If multi-column is enabled, fragment instantiation is handled in MultiColumnSettings.
            assert mMultiColumnSettings == null;

            Fragment fragment = instantiateMainFragment(mPendingNewIntent);
            mPendingNewIntent = null;

            var transaction = getSupportFragmentManager().beginTransaction();
            transaction.setReorderingAllowed(true);
            setFragmentAnimation(transaction, fragment);
            transaction
                    .replace(R.id.content, fragment, MAIN_FRAGMENT_TAG)
                    .addToBackStack(null)
                    .commit();
        }
    }

    private static @SettingsFragment.AnimationType int getAnimationType(Fragment fragment) {
        if (fragment instanceof SettingsFragment settingsFragment) {
            // The fragment is (being) migrated. Respect the animation type that the fragment says.
            return settingsFragment.getAnimationType();
        }

        // The fragment is not yet migrated with auditing. Fallback to the legacy animation type.
        Log.w(TAG, "Non-migrated Settings fragment is found: " + fragment.getClass().getName());
        return SettingsFragment.AnimationType.TWEEN;
    }

    private static void setFragmentAnimation(FragmentTransaction transaction, Fragment fragment) {
        switch (getAnimationType(fragment)) {
            case SettingsFragment.AnimationType.TWEEN -> transaction.setCustomAnimations(
                    R.anim.shared_x_axis_open_enter,
                    R.anim.shared_x_axis_open_exit,
                    R.anim.shared_x_axis_close_enter,
                    R.anim.shared_x_axis_close_exit);
            case SettingsFragment.AnimationType.PROPERTY -> transaction.setCustomAnimations(
                    R.animator.shared_x_axis_open_enter,
                    R.animator.shared_x_axis_open_exit,
                    R.animator.shared_x_axis_close_enter,
                    R.animator.shared_x_axis_close_exit);
        }
    }

    private void checkForMissingDeviceLockOnAutomotive() {
        if (DeviceInfo.isAutomotive()) {
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
        mScrimManager.destroy();
        if (mMultiColumnTitleUpdater != null) {
            assert mMultiColumnSettings != null;
            mMultiColumnSettings.removeObserver(mMultiColumnTitleUpdater);
        }
        if (mTitleUpdater != null) {
            getSupportFragmentManager().unregisterFragmentLifecycleCallbacks(mTitleUpdater);
        }
        if (ChromeFeatureList.sSearchInSettings.isEnabled()) {
            assumeNonNull(mSearchCoordinator).destroy();
        }
        super.onDestroy();
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a {@link
     * PreferenceFragmentCompat}. This does not include dialogs or other {@link Fragment}s shown on
     * top of the main content.
     */
    @VisibleForTesting
    public @Nullable Fragment getMainFragment() {
        if (mMultiColumnSettings == null) {
            return getSupportFragmentManager().findFragmentById(R.id.content);
        }
        return mMultiColumnSettings
                .getChildFragmentManager()
                .findFragmentById(R.id.preferences_detail);
    }

    /** Returns the MultiColumnSettings if it is running in SettingsMultiColumn mode. */
    @VisibleForTesting
    @Nullable MultiColumnSettings getMultiColumnSettings() {
        return mMultiColumnSettings;
    }

    /**
     * Returns the intent request tracker for the Settings Activity. If the tracker does not exist
     * yet create one and return that.
     *
     * @return IntentRequestTracker The intent request tracker for the Settings Activity.
     */
    public IntentRequestTracker getIntentRequestTracker() {
        return mIntentRequestTracker;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (ChromeFeatureList.sSearchInSettings.isEnabled()) {
            assert mSearchCoordinator != null;
            assumeNonNull(mSearchCoordinator).hideHelpAndFeedbackIcon();
            return false;
        }
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
        if (ChromeFeatureList.sSearchInSettings.isEnabled()) {
            return false;
        }
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
            if (item.getItemId() == R.id.menu_id_targeted_help) {
                RecordUserAction.record("Settings.MobileHelpAndFeedback");
            }
            return true;
        }

        if (item.getItemId() == android.R.id.home) {
            if (mMultiColumnSettings != null) {
                if (mMultiColumnSettings.isTwoColumn()) {
                    // In two pane mode, selecting back always exits from the settings activity.
                    finish();
                    return true;
                }
                // PreferenceHeaderFragmentCompat implements back button behavior.
                // In order to forward the event to there, translate the event to the back button.
                onBackPressed();
                return true;
            }
            assumeNonNull(mainFragment);
            finishCurrentSettings(mainFragment);
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            RecordUserAction.record("Settings.MobileHelpAndFeedback");
            HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                    .show(this, getString(R.string.help_context_settings), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mIntentRequestTracker.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mMainFragmentKeyDownHandler != null
                && mMainFragmentKeyDownHandler.onKeyDown(keyCode, event)) {
            return true;
        }
        if (mBottomSheetKeyDownHandler != null
                && mBottomSheetKeyDownHandler.onKeyDown(keyCode, event)) {
            return true;
        }

        // Finish the current settings when the ESC key is pressed.
        if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
            Fragment mainFragment = getMainFragment();
            assumeNonNull(mainFragment);
            finishCurrentSettings(mainFragment);
            return true;
        }
        return super.onKeyDown(keyCode, event);
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
            mMainFragmentKeyDownHandler =
                    BackPressHelper.create(
                            activeFragment.getViewLifecycleOwner(),
                            getOnBackPressedDispatcher(),
                            (BackPressHandler) activeFragment);
        }
    }

    private void registerBottomSheetBackPressHandler() {
        mBottomSheetKeyDownHandler =
                BackPressHelper.create(
                        this,
                        getOnBackPressedDispatcher(),
                        mManagedBottomSheetController.getBottomSheetBackPressHandler());
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        SnackbarManager ret = mSnackbarManagerSupplier.get();
        assert ret != null;
        return ret;
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
        if (!DeviceInfo.isAutomotive()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(this)) {
            return;
        }

        // Use transparent color, so the AppBarLayout can color the status bar on scroll.
        UiUtils.setStatusBarColor(getWindow(), Color.TRANSPARENT);

        // Set status bar icon color according to background color.
        UiUtils.setStatusBarIconColor(
                getWindow().getDecorView().getRootView(),
                getResources().getBoolean(R.bool.window_light_status_bar));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            int color = SemanticColorUtils.getDefaultBgColor(this);
            var taskDescription =
                    new ActivityManager.TaskDescription.Builder().setStatusBarColor(color).build();
            setTaskDescription(taskDescription);
        }
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
     * <p>If the given fragment is not the current one, or the fragment is already finished, this
     * method does nothing. In other words, this method is idempotent.
     *
     * <p>This method executes navigations asynchronously. It means that it is safe to call this
     * method on the UI thread in most cases, particularly even in the middle of executing fragment
     * transactions. On the other hand, you have to be careful when you want to go back multiple
     * pages using this method; it may not work as you expect to call this method multiple times in
     * a row because the subsequent method calls are ignored due to fragment mismatch. Use {@link
     * executePendingNavigations} to synchronously execute pending navigations to work around this
     * problem.
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
        if (mFinishedMainFragment != null && mFinishedMainFragment.get() == fragment) {
            return;
        }

        mFinishedMainFragment = new WeakReference<>(fragment);

        if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
            FragmentManager fragmentManager =
                    mMultiColumnSettings == null
                            ? getSupportFragmentManager()
                            : mMultiColumnSettings.getChildFragmentManager();
            if (fragmentManager.getBackStackEntryCount() == 0) {
                finish();
            } else {
                if (fragmentManager.isStateSaved()) {
                    ++mPendingPopBackCount;
                } else {
                    fragmentManager.popBackStack();
                }
            }
        } else {
            finish();
        }
    }

    /**
     * Executes pending navigations immediately.
     *
     * <p>See {@link finishCurrentSettings} for a valid use case of this method.
     *
     * <p>This method is package-private because it is used by {@link SettingsNavigationImpl}. Use
     * {@link SettingsNavigation} to call this method from fragments, instead of calling it
     * directly.
     */
    void executePendingNavigations() {
        if (ChromeFeatureList.sSettingsSingleActivity.isEnabled()) {
            getSupportFragmentManager().executePendingTransactions();
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

        private @Nullable ObservableSupplier<String> mCurrentPageTitle;

        @Override
        public void onFragmentStarted(FragmentManager fragmentManager, Fragment fragment) {
            assert mMultiColumnSettings == null;
            if (!MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            // TitleUpdater is enabled only when the fragment implements EmbeddableSettingsPage.
            EmbeddableSettingsPage settingsFragment = (EmbeddableSettingsPage) fragment;

            if (mCurrentPageTitle != null) {
                mCurrentPageTitle.removeObserver(mSetTitleCallback);
            }
            mCurrentPageTitle = settingsFragment.getPageTitle();
            mCurrentPageTitle.addSyncObserverAndCallIfNonNull(mSetTitleCallback);
        }
    }

    private class WideDisplayPaddingApplier extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                FragmentManager fragmentManager,
                Fragment fragment,
                View view,
                @Nullable Bundle savedInstanceState) {
            if (fragment instanceof PreferenceFragmentCompat
                    || MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                // TODO(crbug.com/439911511): Have this logic in the same place as other layout
                // updates
                view.getViewTreeObserver()
                        .addOnGlobalLayoutListener(
                                new ViewTreeObserver.OnGlobalLayoutListener() {
                                    @Override
                                    public void onGlobalLayout() {
                                        if (fragment.getView() == null) return;
                                        fragment.getView()
                                                .getViewTreeObserver()
                                                .removeOnGlobalLayoutListener(this);
                                        WideDisplayPadding.apply(fragment, SettingsActivity.this);
                                    }
                                });
            }
        }
    }

    private static class SettingsMetricsReporter
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentAttached(
                FragmentManager fragmentManager, Fragment fragment, Context context) {
            if (!(fragment instanceof SettingsFragment)
                    && !MAIN_FRAGMENT_TAG.equals(fragment.getTag())) {
                return;
            }

            String className = fragment.getClass().getSimpleName();
            RecordHistogram.recordSparseHistogram(
                    "Settings.FragmentAttached", className.hashCode());
            // Log hashCode to easily add new class names to enums.xml.
            Log.d(
                    TAG,
                    String.format(
                            Locale.ENGLISH,
                            "Settings.FragmentAttached: <int value=\"%d\" label=\"%s\"/>",
                            className.hashCode(),
                            className));

            if (!(fragment instanceof SettingsFragment)) {
                RecordHistogram.recordSparseHistogram(
                        "Settings.NonSettingsFragmentAttached", className.hashCode());
                Log.e(
                        TAG,
                        String.format(
                                Locale.ENGLISH,
                                "%s does not implement SettingsFragment",
                                className));
            }
            assert fragment instanceof SettingsFragment
                    : className + "does not implement SettingsFragment";
        }
    }
}
