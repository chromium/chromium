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

    @SuppressWarnings("unused")
    private final Tab mTab;

    private @Nullable SettingsHostFragment mSettingsHostFragment;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mTitleUpdaterLifecycleCallbacks;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mSettingsMetricsReporter;
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

        // Inflate the settings layout into the container view. Ensure it has the right theme.
        // TODO(crbug.com/521895796): Rename settings_activity.xml since with settings-in-a-tab it
        // doesn't map directly to its own activity.
        Context themedContext = new ContextThemeWrapper(mActivity, R.style.Theme_Chromium_Settings);
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
        SettingsMenuHelper.onCreateOptionsMenu(mToolbar.getMenu(), mActivity);
        SettingsMenuHelper.onPrepareOptionsMenu(mToolbar.getMenu());
        mToolbar.setOnMenuItemClickListener(
                item -> SettingsMenuHelper.onOptionsItemSelected(item, mActivity, this));

        mSettingsHostFragment =
                (SettingsHostFragment) fragmentManager.findFragmentByTag(mFragmentTag);
        if (mSettingsHostFragment == null) {
            mSettingsHostFragment = new SettingsHostFragment();
            fragmentManager
                    .beginTransaction()
                    .add(fragmentContainer.getId(), mSettingsHostFragment, mFragmentTag)
                    .commitAllowingStateLoss();
        }
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
        mSettingsHostFragment.setDependencyProvider(dependencyProvider);
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
        // TODO(crbug.com/531873184): Called when the tab's URL changes, so handle
        // mPendingUrl state, as well as showing the corresponding fragment
        // via mSettingsHostFragment.
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

        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null) {
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
            fragmentManager
                    .beginTransaction()
                    .remove(mSettingsHostFragment)
                    .commitAllowingStateLoss();
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
                        mActivity,
                        titleContainer,
                        mToolbar::setTitle,
                        this::onTitleTapped,
                        mInitialBreadcrumbPath);
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
        return mSettingsHostFragment.getActiveFragment();
    }

    @Override
    public @Nullable MultiColumnSettings getMultiColumnSettings() {
        return (MultiColumnSettings) getMainFragment();
    }

    @Override
    public @Nullable SettingsSearchCoordinator getSearchCoordinator() {
        return mSearchCoordinator;
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
        assert mSearchCoordinator == null;
        assert mToolbar != null;
        assert mSettingsHostFragment != null;
        SettingsContainmentHelper containmentHelper = mSettingsHostFragment.getContainmentHelper();
        assert containmentHelper != null;

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
        updateBackPressState();
    }

    @Override
    public void onSlideStateUpdated(int newState) {
        updateNavigationIcon();
        updateBackPressState();
    }

    @Override
    public void onHeaderLayoutUpdated() {
        updateNavigationIcon();
        updateBackPressState();
    }

    private void updateNavigationIcon() {
        if (mToolbar != null) {
            // The layout must be updated at least once before isTwoColumnSettingsVisible() returns
            // the correct value.
            SettingsMenuHelper.updateNavigationIcon(
                    mToolbar,
                    mActivity,
                    /* show= */ true,
                    isTwoColumnSettingsVisible(),
                    isMainSettingsVisible());
        }
    }

    private boolean isMainSettingsVisible() {
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        return multiColumnSettings != null && !multiColumnSettings.isLayoutOpen();
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
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null) {
            if (multiColumnSettings.getBackStackEntryCount() > 0) {
                multiColumnSettings.popBackStack();
                return BackPressResult.SUCCESS;
            }
            if (multiColumnSettings.getView() != null) {
                var slidingPane = multiColumnSettings.getSlidingPaneLayout();
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
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        if (multiColumnSettings != null) {
            if (multiColumnSettings.getBackStackEntryCount() > 0) {
                canHandle = true;
            } else if (multiColumnSettings.getView() != null) {
                var slidingPane = multiColumnSettings.getSlidingPaneLayout();
                if (slidingPane != null && slidingPane.isSlideable() && slidingPane.isOpen()) {
                    canHandle = true;
                }
            }
        } else if (mSettingsHostFragment != null && mSettingsHostFragment.isAttachedToActivity()) {
            canHandle = mSettingsHostFragment.getBackStackEntryCount() > 0;
        }
        mBackPressStateSupplier.set(canHandle);
    }

    /** Utility class to handle creating the title updater and deferred URL navigation. */
    private class TitleUpdaterLifecycleCallbacks
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                FragmentManager fm, Fragment f, View v, @Nullable Bundle savedFragmentState) {
            if (f instanceof MultiColumnSettings multiColumnSettings) {
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
