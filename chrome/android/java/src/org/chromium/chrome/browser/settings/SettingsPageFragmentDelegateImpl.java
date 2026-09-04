// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcherProvider;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Implementation of {@link SettingsPage.FragmentDelegate} that manages {@link SettingsPage}
 * fragments. Exists because {@link SettingsPage} is at a lower level in the dependency graph than
 * some of the dependencies needed by {@link FragmentDependencyProvider}.
 */
@NullMarked
public class SettingsPageFragmentDelegateImpl
        implements SettingsPage.FragmentDelegate,
                SettingsMenuHelper.Delegate,
                PreferenceUpdateObserver,
                MultiColumnSettings.Observer,
                SaveInstanceStateObserver,
                BackPressHandler {
    private static final String SETTINGS_NATIVE_PAGE_TAG = "settings_native_page";

    private final Activity mActivity;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final SnackbarManager mSnackbarManager;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateSupplier;
    private final String mFragmentTag;
    private final Tab mTab;

    private @Nullable SettingsHostFragment mSettingsHostFragment;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mTitleUpdaterLifecycleCallbacks;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mSettingsMetricsReporter;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mOptionsMenuLifecycleCallbacks;
    private @Nullable Toolbar mToolbar;
    private @Nullable MultiColumnTitleUpdater mMultiColumnTitleUpdater;
    private @Nullable SettingsSearchCoordinator mSearchCoordinator;
    private @Nullable ComponentCallbacks mComponentCallbacks;
    private @Nullable List<SettingsIndexData.Entry> mInitialBreadcrumbPath;
    private @Nullable String mPendingUrl;

    public SettingsPageFragmentDelegateImpl(
            Activity activity,
            Profile profile,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            Tab tab) {
        assert ChromeFeatureList.sSettingsInTab.isEnabled()
                : "SettingsInTab feature must be enabled to use this class.";
        mActivity = activity;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mModalDialogSupplier = ObservableSuppliers.<ModalDialogManager>createMonotonic();
        mModalDialogSupplier.set(mModalDialogManager);
        mBackPressStateSupplier = ObservableSuppliers.createNonNull(false);
        mTab = tab;
        // Ensure fragment has a globally unique tag so new settings tabs don't collide with
        // existing settings tabs (or closing tabs in the undo close tab snackbar queue). Use
        // tab.getId() because it is stable across Activity restarts (e.g. theme changes).
        mFragmentTag = SETTINGS_NATIVE_PAGE_TAG + "_" + tab.getId();
    }

    @Override
    public void initSettings(ViewGroup containerView, String initialUrl) {
        initSettingsInternal(containerView, initialUrl, /* attachToContainer= */ false);
    }

    /**
     * Initializes settings and attaches the host fragment directly to the container for testing in
     * standalone test activities (like {@code SettingsInTabTestActivity}) where there is only one
     * tab, multi-tab recreation is not a concern, and tests expect synchronous initialization.
     */
    public void initSettingsForTesting(ViewGroup containerView, String initialUrl) {
        initSettingsInternal(containerView, initialUrl, /* attachToContainer= */ true);
    }

    private void initSettingsInternal(
            ViewGroup containerView, String initialUrl, boolean attachToContainer) {
        if (!initialUrl.isEmpty()) {
            mPendingUrl = initialUrl;
        }

        FragmentManager fragmentManager =
                ((FragmentActivity) mActivity).getSupportFragmentManager();

        // Create the dependency provider for settings fragments.
        OneshotSupplierImpl<WindowAndroid> windowAndroidSupplier = new OneshotSupplierImpl<>();
        windowAndroidSupplier.set(mWindowAndroid);

        OneshotSupplierImpl<SnackbarManager> snackbarSupplier = new OneshotSupplierImpl<>();
        snackbarSupplier.set(mSnackbarManager);

        OneshotSupplierImpl<BottomSheetController> bottomSheetSupplier =
                new OneshotSupplierImpl<>();
        bottomSheetSupplier.set(mBottomSheetController);

        // Update the search coordinator on configuration change.
        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        if (mSearchCoordinator != null) {
                            mSearchCoordinator.onConfigurationChanged(newConfig);
                        }
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mActivity.registerComponentCallbacks(mComponentCallbacks);

        // TODO(crbug.com/521895796): Used for settings fragments that are shown using a
        // new activity, where we want to apply padding and record histograms. Sort out if
        // there are any such fragment left and provide a non-null tag here if so.
        @Nullable String mainFragmentTag = null;

        mSettingsMetricsReporter = new SettingsMetricsReporter(mainFragmentTag);
        fragmentManager.registerFragmentLifecycleCallbacks(
                mSettingsMetricsReporter, /* recursive= */ true);

        mOptionsMenuLifecycleCallbacks =
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentResumed(FragmentManager fm, Fragment f) {
                        updateOptionsMenu();
                    }
                };
        fragmentManager.registerFragmentLifecycleCallbacks(
                mOptionsMenuLifecycleCallbacks, /* recursive= */ true);

        // Inflate the settings layout into the container view. Ensure it has the right theme.
        // TODO(crbug.com/521895796): Rename settings_activity.xml since with settings-in-a-tab it
        // doesn't map directly to its own activity.
        Context themedContext =
                new ContextThemeWrapper(mActivity, R.style.ThemeOverlay_Chromium_Settings);
        View settingsView =
                LayoutInflater.from(themedContext).inflate(R.layout.settings_activity, null);

        // SettingsInTab uses the root BottomSheetController and ModalDialogManager from
        // ChromeTabbedActivity, so remove the unused local containers to prevent duplicate
        // R.id.sheet_container or R.id.dialog_container instances.
        View sheetContainer = settingsView.findViewById(R.id.sheet_container);
        assert sheetContainer != null;
        UiUtils.removeViewFromParent(sheetContainer);
        View dialogContainer = settingsView.findViewById(R.id.dialog_container);
        assert dialogContainer != null;
        UiUtils.removeViewFromParent(dialogContainer);

        containerView.addView(settingsView);
        ViewGroup fragmentContainer = settingsView.findViewById(R.id.settings_content);
        mToolbar = settingsView.findViewById(R.id.action_bar);

        // Apply semantic colors to the top-level container and app bar.
        int backgroundColor = SemanticColorUtils.getSettingsBackgroundColor(mActivity);
        fragmentContainer.setBackgroundColor(backgroundColor);
        AppBarLayout appBarLayout = settingsView.findViewById(R.id.app_bar_layout);
        appBarLayout.setBackgroundColor(backgroundColor);
        appBarLayout.setElevation(0);
        appBarLayout.setStateListAnimator(null);
        int topPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.settings_top_padding);
        appBarLayout.setPaddingRelative(0, topPadding, 0, 0);

        // Set the "Settings" label. The icon is updated in OnHeaderLayoutUpdated(), after layout
        // has determined whether settings is one-column or two-column.
        mToolbar.setTitle(R.string.settings);

        // Set up Help Menu on Toolbar.
        updateOptionsMenu();
        mToolbar.setOnMenuItemClickListener(
                item -> SettingsMenuHelper.onOptionsItemSelected(item, mActivity, this));

        var dependencyProvider =
                new FragmentDependencyProvider(
                        mActivity,
                        mProfile,
                        windowAndroidSupplier,
                        mActivityResultTracker,
                        snackbarSupplier,
                        bottomSheetSupplier,
                        mModalDialogSupplier,
                        () -> mSearchCoordinator);

        mSettingsHostFragment =
                (SettingsHostFragment) fragmentManager.findFragmentByTag(mFragmentTag);
        if (mSettingsHostFragment == null) {
            mSettingsHostFragment = new SettingsHostFragment();
            // Set the dependency provider before executing the transaction so child fragments
            // created during attachment (e.g. MainSettings) have their dependencies attached
            // before creating their preferences.
            mSettingsHostFragment.setDependencyProvider(dependencyProvider);
            if (attachToContainer) {
                // In standalone test activities, attach directly to the container because tests
                // expect immediate view attachment.
                fragmentManager
                        .beginTransaction()
                        .add(fragmentContainer.getId(), mSettingsHostFragment, mFragmentTag)
                        .commitAllowingStateLoss();
            } else {
                // Add the fragment without a container using two-parameter add() to prevent
                // multiple settings tabs from colliding on the same container ID during activity
                // recreation.
                fragmentManager
                        .beginTransaction()
                        .add(mSettingsHostFragment, mFragmentTag)
                        .commitAllowingStateLoss();
            }
            // Execute the transaction so mSettingsHostFragment creates its view and getView() is
            // non-null below.
            fragmentManager.executePendingTransactions();
        } else {
            mSettingsHostFragment.setDependencyProvider(dependencyProvider);
        }
        mSettingsHostFragment.setSaveInstanceStateCallback(this::onSaveInstanceState);

        // If the host fragment view was attached to a different tab's container, attach it to this
        // tab's container instead.
        View hostView = mSettingsHostFragment.getView();
        if (hostView != null && hostView.getParent() != fragmentContainer) {
            UiUtils.removeViewFromParent(hostView);
            LayoutParams layoutParams =
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            fragmentContainer.addView(hostView, layoutParams);
        }

        if (ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
            mSettingsHostFragment.setSettingsNavigation(new SettingsInTabNavigationDelegate(mTab));
            if (mTab.getUrl() != null && !mTab.getUrl().isEmpty()) {
                String restoredUrl = mTab.getUrl().getSpec();
                if (restoredUrl != null && !restoredUrl.isEmpty()) {
                    // Capture and apply the restored tab URL (e.g. "chrome://settings/appearance")
                    // during tab initialization or session restore to synchronize the displayed
                    // settings fragment with the restored WebContents URL.
                    updateForUrl(restoredUrl);
                }
            }
        }

        if (mSettingsHostFragment.isAdded()) {
            mSettingsHostFragment
                    .getChildFragmentManager()
                    .addOnBackStackChangedListener(this::updateBackPressState);
        }

        assert mActivity instanceof ActivityLifecycleDispatcherProvider;
        ((ActivityLifecycleDispatcherProvider) mActivity).getLifecycleDispatcher().register(this);

        // Compute initial breadcrumb path.
        Bundle savedInstanceState = getSavedInstanceState();
        if (savedInstanceState == null) {
            Intent intent = mActivity.getIntent();
            mInitialBreadcrumbPath =
                    SettingsBreadcrumbUtil.getInitialBreadcrumbPath(
                            /* context= */ mActivity,
                            assertNonNull(mProfile),
                            intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT),
                            intent.getBundleExtra(
                                    SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS));
        } else {
            mInitialBreadcrumbPath =
                    SettingsBreadcrumbUtil.getInitialBreadcrumbPath(savedInstanceState);
        }

        // Set initial navigation icon early to prevent delayed appearance on slow devices.
        updateNavigationIcon();

        // During activity recreation savedInstanceState may be non-null but MultiColumnSettings may
        // not be attached yet. Check for the existing of MultiColumnSettings to decide whether to
        // create the title updater and search coordinator now vs. later.
        // TODO(crbug.com/537343764): Revisit this and make it more similar to SettingsActivity's
        // initialization behavior.
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null && multiColumnSettings.getView() != null) {
            createMultiColumnTitleUpdater(
                    multiColumnSettings, multiColumnSettings.requireView(), savedInstanceState);
            createSearchCoordinator(multiColumnSettings, savedInstanceState);
            multiColumnSettings.addObserver(this);
            if (multiColumnSettings.isAdded()) {
                multiColumnSettings
                        .getChildFragmentManager()
                        .addOnBackStackChangedListener(this::updateBackPressState);
            }
            onHeaderLayoutUpdated();
        } else {
            // Otherwise create the title updater and search coordinator when the fragment is
            // created.
            mTitleUpdaterLifecycleCallbacks = new TitleUpdaterLifecycleCallbacks();
            fragmentManager.registerFragmentLifecycleCallbacks(
                    mTitleUpdaterLifecycleCallbacks, /* recursive= */ true);
        }
    }

    @Override
    public void updateForUrl(String url) {
        if (!ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) return;
        if (mSettingsHostFragment == null) return;

        // If MultiColumnSettings or its view hierarchy is not created yet
        // (e.g. during initial NativePage construction before FragmentManager
        // transaction commit completes), defer the URL update until
        // onFragmentViewCreated via TitleUpdaterLifecycleCallbacks.
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings == null || multiColumnSettings.getView() == null) {
            mPendingUrl = url;
            mSettingsHostFragment.setInitialUrl(url);
            return;
        }

        mPendingUrl = null;

        var fragmentClass = SettingsFragmentRegistry.getFragmentClassForUrl(url);
        if (fragmentClass == null) {
            fragmentClass = MainSettings.class;
        }

        // If navigating to root chrome://settings URL (e.g. via Omnibox),
        // clear any stored initial subpage URL on attached host fragment
        // so that resetting the pane loads the default Account fragment
        // without falling back to a stale initial URL.
        if (MainSettings.class.equals(fragmentClass)) {
            mSettingsHostFragment.clearInitialUrl();
        }

        Bundle args = SettingsFragmentRegistry.parseUrlArguments(url);
        Fragment fragment = null;
        if (!MainSettings.class.equals(fragmentClass)) {
            fragment = Fragment.instantiate(mActivity, fragmentClass.getName(), args);
        }

        // Transactions pass addToBackStack = false because browser backstack
        // history is strictly managed by WebContents and navigation controller
        // entries.
        mSettingsHostFragment.showFragment(fragment, /* addToBackStack= */ false, /* tag= */ null);
    }

    @Override
    public void destroySettings() {
        assert mActivity instanceof ActivityLifecycleDispatcherProvider;
        ((ActivityLifecycleDispatcherProvider) mActivity).getLifecycleDispatcher().unregister(this);

        FragmentManager fragmentManager =
                ((FragmentActivity) mActivity).getSupportFragmentManager();

        if (mTitleUpdaterLifecycleCallbacks != null) {
            fragmentManager.unregisterFragmentLifecycleCallbacks(mTitleUpdaterLifecycleCallbacks);
            mTitleUpdaterLifecycleCallbacks = null;
        }

        assumeNonNull(mSettingsMetricsReporter);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mSettingsMetricsReporter);
        mSettingsMetricsReporter = null;

        if (mOptionsMenuLifecycleCallbacks != null) {
            fragmentManager.unregisterFragmentLifecycleCallbacks(mOptionsMenuLifecycleCallbacks);
            mOptionsMenuLifecycleCallbacks = null;
        }

        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null) {
            multiColumnSettings.setOnCreateViewRunnable(null);
            if (mMultiColumnTitleUpdater != null) {
                multiColumnSettings.removeObserver(mMultiColumnTitleUpdater);
            }
            if (mSearchCoordinator != null) {
                multiColumnSettings.removeObserver(mSearchCoordinator);
            }
            multiColumnSettings.removeObserver(this);
        }
        mMultiColumnTitleUpdater = null;

        if (mSearchCoordinator != null) {
            mSearchCoordinator.destroy();
            mSearchCoordinator = null;
        }

        if (mComponentCallbacks != null) {
            mActivity.unregisterComponentCallbacks(mComponentCallbacks);
            mComponentCallbacks = null;
        }

        if (mSettingsHostFragment != null) {
            boolean isUrlNavEnabled = ChromeFeatureList.sSettingsInTabUrlNav.isEnabled();
            // Because the SettingsHostFragment is identified by mFragmentTag (derived from Tab ID),
            // a new SettingsPage instantiated for the same Tab will adopt this fragment. If the
            // Tab's current native page is a SettingsPage, it means this Tab has already
            // transitioned to a new SettingsPage instance that has adopted our fragment. This can
            // happen during url navigation after a chrome session has been restored (e.g.,
            // navigating to a previous page in the Chrome navigation stack). In that case, do not
            // destroy the shared host fragment or clear its callbacks.
            boolean isAdoptedByNewPage =
                    isUrlNavEnabled && mTab.getNativePage() instanceof SettingsPage;
            if (!isAdoptedByNewPage) {
                mSettingsHostFragment.setSaveInstanceStateCallback(null);
                if (isUrlNavEnabled) {
                    mSettingsHostFragment.setSettingsNavigation(null);
                }
                fragmentManager
                        .beginTransaction()
                        .remove(mSettingsHostFragment)
                        .commitAllowingStateLoss();
            }
        }
        mSettingsHostFragment = null;
        mToolbar = null;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (mSearchCoordinator != null) {
            mSearchCoordinator.onSaveInstanceState(outState);
        }
        if (mMultiColumnTitleUpdater != null) {
            mMultiColumnTitleUpdater.onSaveInstanceState(outState);
        }
        SettingsBreadcrumbUtil.saveInitialBreadcrumbPath(outState, mInitialBreadcrumbPath);
    }

    @Override
    public void onSaveInstanceState(Bundle outState, PersistableBundle outPersistentState) {}

    private @Nullable Bundle getSavedInstanceState() {
        // Restore per-tab settings state (e.g. search coordinator, title updater, breadcrumbs)
        // from this host fragment's bundle so multiple settings tabs don't collide in the
        // Activity's shared saved instance state during Activity recreation (such as theme
        // changes).
        if (mSettingsHostFragment != null
                && mSettingsHostFragment.getSavedInstanceState() != null) {
            return mSettingsHostFragment.getSavedInstanceState();
        }
        return mActivity instanceof AsyncInitializationActivity asyncActivity
                ? asyncActivity.getSavedInstanceState()
                : null;
    }

    private void createMultiColumnTitleUpdater(
            MultiColumnSettings multiColumnSettings,
            View view,
            @Nullable Bundle savedInstanceState) {
        assert mMultiColumnTitleUpdater == null;

        LinearLayout titleContainer = view.findViewById(R.id.settings_title_in_detailed_pane);
        assumeNonNull(titleContainer);
        assumeNonNull(mToolbar);

        mMultiColumnTitleUpdater =
                new MultiColumnTitleUpdater(
                        savedInstanceState,
                        multiColumnSettings,
                        titleContainer,
                        mToolbar::setTitle,
                        this::onTitleTapped,
                        mInitialBreadcrumbPath,
                        this::updateBackPressState);
        multiColumnSettings.addObserver(mMultiColumnTitleUpdater);
    }

    private void onTitleTapped(@Nullable String entryName) {
        SettingsSearchCoordinator searchCoordinator = getSearchCoordinator();
        if (searchCoordinator != null) {
            searchCoordinator.onTitleTapped(entryName);
        }
    }

    @Override
    public @Nullable Fragment getMainFragment() {
        // Allows tests to simulate activity attachment behavior.
        if (mSettingsHostFragment == null || !mSettingsHostFragment.isAttachedToActivity()) {
            return null;
        }
        return mSettingsHostFragment.getMainFragment();
    }

    @Override
    public @Nullable MultiColumnSettings getMultiColumnSettings() {
        if (mSettingsHostFragment == null || !mSettingsHostFragment.isAttachedToActivity()) {
            return null;
        }
        return mSettingsHostFragment.getMultiColumnSettings();
    }

    @Override
    public @Nullable SettingsSearchCoordinator getSearchCoordinator() {
        return mSearchCoordinator;
    }

    void setSearchCoordinatorForTesting(@Nullable SettingsSearchCoordinator searchCoordinator) {
        mSearchCoordinator = searchCoordinator;
    }

    void setMultiColumnTitleUpdaterForTesting(
            @Nullable MultiColumnTitleUpdater multiColumnTitleUpdater) {
        mMultiColumnTitleUpdater = multiColumnTitleUpdater;
    }

    @Override
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        return HelpAndFeedbackLauncherFactory.getForProfile(mProfile);
    }

    @Override
    public void finishSettings() {
        // TODO(crbug.com/521895796): Define settings-in-tab close behavior.
    }

    @Override
    public void onBackPressed() {
        mActivity.onBackPressed();
    }

    @Override
    public void finishCurrentSettings(Fragment fragment) {
        assert mSettingsHostFragment != null;
        assert mSettingsHostFragment.isAttachedToActivity();
        mSettingsHostFragment.finishCurrentSettings(fragment);
    }

    public boolean isTwoColumnSettingsVisible() {
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        return multiColumnSettings != null && multiColumnSettings.isTwoColumn();
    }

    @Override
    public void onPreferencesUpdated(PreferenceFragmentCompat fragment) {
        if (mSettingsHostFragment != null) {
            mSettingsHostFragment.onPreferencesUpdated(fragment);
        }
    }

    private void createSearchCoordinator(
            MultiColumnSettings multiColumnSettings, @Nullable Bundle savedInstanceState) {
        // mSearchCoordinator may already be non-null if initialized during initSettings() before
        // TitleUpdaterLifecycleCallbacks fired, or during activity recreation / theme change.
        if (mSearchCoordinator != null) return;

        assert mToolbar != null;
        assert mSettingsHostFragment != null;

        // containmentHelper can be null if mSettingsHostFragment is detached or not yet attached
        // during activity recreation / theme changes.
        SettingsContainmentHelper containmentHelper = mSettingsHostFragment.getContainmentHelper();
        if (containmentHelper == null) {
            // SearchCoordinator requires containment item decorations to properly style search
            // result highlights with rounded corners. Delay creation until SettingsHostFragment is
            // attached and containmentHelper is available.
            FragmentManager fragmentManager =
                    ((FragmentActivity) mActivity).getSupportFragmentManager();
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new FragmentManager.FragmentLifecycleCallbacks() {
                        @Override
                        public void onFragmentAttached(
                                FragmentManager fm, Fragment f, Context context) {
                            if (f == mSettingsHostFragment) {
                                fm.unregisterFragmentLifecycleCallbacks(this);
                                createSearchCoordinator(multiColumnSettings, savedInstanceState);
                            }
                        }
                    },
                    /* recursive= */ false);
            return;
        }

        mSearchCoordinator =
                new SettingsSearchCoordinator(
                        (FragmentActivity) mActivity,
                        mToolbar,
                        this::isTwoColumnSettingsVisible,
                        multiColumnSettings,
                        containmentHelper.getItemDecorations(),
                        mProfile,
                        this::updateFirstVisibleTitle,
                        mModalDialogSupplier);

        // Multi column settings may have already created its view (in case of Activity
        // re-creation), so initialize the search coordinator's view if it exists.
        if (multiColumnSettings.getView() != null) {
            mSearchCoordinator.initializeSearchUi(savedInstanceState);
        } else {
            multiColumnSettings.setOnCreateViewRunnable(
                    () -> assumeNonNull(mSearchCoordinator).initializeSearchUi(savedInstanceState));
        }
        multiColumnSettings.addObserver(mSearchCoordinator);
    }

    private void updateFirstVisibleTitle(int index) {
        assumeNonNull(mMultiColumnTitleUpdater).setFirstVisibleTitleIndex(index);
    }

    @Override
    public void onTitleUpdated() {
        updateNavigationIcon();
        updateOptionsMenu();
        updateBackPressState();
    }

    @Override
    public void onSlideStateUpdated(int newState) {
        updateNavigationIcon();
        updateOptionsMenu();
        updateBackPressState();
    }

    @Override
    public void onHeaderLayoutUpdated() {
        if (mSettingsHostFragment != null) {
            mSettingsHostFragment.updateContainmentForAttachedFragments();
        }
        updateNavigationIcon();
        updateOptionsMenu();
        updateBackPressState();
    }

    private void updateOptionsMenu() {
        if (mToolbar != null) {
            SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, this);
            if (mSearchCoordinator != null) {
                mSearchCoordinator.updateHelpMenuVisibility();
            }
        }
    }

    private void updateNavigationIcon() {
        if (mToolbar != null) {
            // The layout must be updated at least once before isTwoColumnSettingsVisible() returns
            // the correct value.
            SettingsMenuHelper.updateNavigationIcon(
                    mToolbar,
                    mActivity,
                    /* show= */ shouldShowNavigationIcon(),
                    isTwoColumnSettingsVisible(),
                    isMainSettingsVisible());
        }
    }

    private boolean shouldShowNavigationIcon() {
        return mSearchCoordinator == null || mSearchCoordinator.shouldShowNavigationIcon();
    }

    private boolean isMainSettingsVisible() {
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        // If MultiColumnSettings is attached, use it.
        if (multiColumnSettings != null) {
            return !multiColumnSettings.isLayoutOpen();
        }
        // Before MultiColumnSettings is created or attached, check if an initial deep-link
        // breadcrumb path exists. If not (or size <= 1), top-level main settings is being shown.
        return mInitialBreadcrumbPath == null
                || mInitialBreadcrumbPath.isEmpty()
                || mInitialBreadcrumbPath.size() <= 1;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (mSearchCoordinator != null && mSearchCoordinator.handleBackAction()) {
            return BackPressResult.SUCCESS;
        }
        if (mMultiColumnTitleUpdater != null && mMultiColumnTitleUpdater.handleBackAction()) {
            return BackPressResult.SUCCESS;
        }
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null) {
            if (multiColumnSettings.getBackStackEntryCount() > 0) {
                multiColumnSettings.popBackStack();
                return BackPressResult.SUCCESS;
            }
            // When Url Navigation is enabled, the back press should not close the sliding
            // pane, instead the back press should route to the Chrome navigation stack.
            // This keeps the UI in-sync with the Url, while keep compatibility with the
            // old navigation stack (e.g., still used for search results)
            if (!ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
                var slidingPane = multiColumnSettings.getSlidingPaneLayoutOrNull();
                if (slidingPane != null && slidingPane.isSlideable() && slidingPane.isOpen()) {
                    slidingPane.closePane();
                    return BackPressResult.SUCCESS;
                }
            }
        }
        if (mSettingsHostFragment != null && mSettingsHostFragment.isAttachedToActivity()) {
            if (mSettingsHostFragment.getBackStackEntryCount() > 0) {
                mSettingsHostFragment.popBackStack();
                return BackPressResult.SUCCESS;
            }
        }
        return BackPressResult.FAILURE;
    }

    private void updateBackPressState() {
        boolean canHandle = false;
        if (mMultiColumnTitleUpdater != null && mMultiColumnTitleUpdater.isSearchOpen()) {
            canHandle = true;
        } else {
            MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
            if (multiColumnSettings != null) {
                if (multiColumnSettings.getBackStackEntryCount() > 0) {
                    canHandle = true;
                } else if (!ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
                    // A back press should route through the Chrome navigation stack instead of
                    // handling the slidingPaneLayout to keep the contents in-sync with the Url.
                    var slidingPane = multiColumnSettings.getSlidingPaneLayoutOrNull();
                    if (slidingPane != null && slidingPane.isSlideable() && slidingPane.isOpen()) {
                        canHandle = true;
                    }
                }
            } else if (mSettingsHostFragment != null
                    && mSettingsHostFragment.isAttachedToActivity()) {
                canHandle = mSettingsHostFragment.getBackStackEntryCount() > 0;
            }
        }
        mBackPressStateSupplier.set(canHandle);
    }

    /** Utility class to handle creating the title updater and deferred URL navigation. */
    private class TitleUpdaterLifecycleCallbacks
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                FragmentManager fm, Fragment f, View v, @Nullable Bundle savedFragmentState) {
            if (!(f instanceof MultiColumnSettings multiColumnSettings)) return;

            // Ensure the MultiColumnSettings instance belongs to this tab's SettingsHostFragment
            // so we don't bind to fragments created for other settings tabs.
            if (mSettingsHostFragment == null
                    || mSettingsHostFragment.containsChild(multiColumnSettings)) {
                Bundle savedInstanceState = getSavedInstanceState();
                createMultiColumnTitleUpdater(multiColumnSettings, v, savedInstanceState);
                createSearchCoordinator(multiColumnSettings, savedInstanceState);
                multiColumnSettings.addObserver(SettingsPageFragmentDelegateImpl.this);
                if (multiColumnSettings.isAdded()) {
                    multiColumnSettings
                            .getChildFragmentManager()
                            .addOnBackStackChangedListener(
                                    SettingsPageFragmentDelegateImpl.this::updateBackPressState);
                }
                updateBackPressState();

                assert mTitleUpdaterLifecycleCallbacks == this;
                fm.unregisterFragmentLifecycleCallbacks(mTitleUpdaterLifecycleCallbacks);
                mTitleUpdaterLifecycleCallbacks = null;

                if (mPendingUrl != null) {
                    String pendingUrl = mPendingUrl;
                    mPendingUrl = null;
                    updateForUrl(pendingUrl);
                }
            }
        }
    }
}
