// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.os.Bundle;
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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Implementation of {@link SettingsPage.FragmentDelegate} that manages {@link SettingsPage}
 * fragments. Exists because {@link SettingsPage} is at a lower level in the dependency graph than
 * some of the dependencies needed by {@link FragmentDependencyProvider}.
 */
@NullMarked
public class SettingsPageFragmentDelegateImpl
        implements SettingsPage.FragmentDelegate,
                SettingsMenuHelper.Delegate,
                ContainmentHelper.Delegate,
                PreferenceUpdateObserver {
    private static final String SETTINGS_NATIVE_PAGE_TAG = "settings_native_page";

    private final Activity mActivity;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final SnackbarManager mSnackbarManager;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogSupplier;
    private final ContainmentHelper mContainmentHelper;

    private @Nullable SettingsHostFragment mSettingsHostFragment;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mDependencyProvider;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mTitleUpdaterLifecycleCallbacks;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mWideDisplayPaddingApplier;
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mSettingsMetricsReporter;
    private @Nullable Toolbar mToolbar;
    private @Nullable MultiColumnTitleUpdater mMultiColumnTitleUpdater;
    private @Nullable SettingsSearchCoordinator mSearchCoordinator;
    private @Nullable ComponentCallbacks mComponentCallbacks;

    public SettingsPageFragmentDelegateImpl(
            Activity activity,
            Profile profile,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager) {
        mActivity = activity;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mModalDialogSupplier = ObservableSuppliers.<ModalDialogManager>createMonotonic();
        mModalDialogSupplier.set(mModalDialogManager);
        mContainmentHelper = new ContainmentHelper(activity, this);
    }

    @Override
    public void initSettings(ViewGroup containerView) {
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

        mDependencyProvider =
                new FragmentDependencyProvider(
                        mActivity,
                        mProfile,
                        windowAndroidSupplier,
                        mActivityResultTracker,
                        snackbarSupplier,
                        bottomSheetSupplier,
                        mModalDialogSupplier,
                        () -> mSearchCoordinator);

        fragmentManager.registerFragmentLifecycleCallbacks(
                mDependencyProvider, /* recursive= */ true);

        mContainmentHelper.registerCallbacks(fragmentManager);

        mTitleUpdaterLifecycleCallbacks = new TitleUpdaterLifecycleCallbacks();
        fragmentManager.registerFragmentLifecycleCallbacks(
                mTitleUpdaterLifecycleCallbacks, /* recursive= */ true);

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

        mWideDisplayPaddingApplier =
                new WideDisplayPaddingApplier(
                        mActivity, this::isTwoColumnSettingsVisible, mainFragmentTag);
        fragmentManager.registerFragmentLifecycleCallbacks(
                mWideDisplayPaddingApplier, /* recursive= */ true);

        mSettingsMetricsReporter = new SettingsMetricsReporter(mainFragmentTag);
        fragmentManager.registerFragmentLifecycleCallbacks(
                mSettingsMetricsReporter, /* recursive= */ true);

        // Inflate the settings layout into the container view.
        // TODO(crbug.com/521895796): Rename settings_activity.xml since with settings-in-a-tab it
        // doesn't map directly to its own activity.
        View settingsView =
                LayoutInflater.from(mActivity).inflate(R.layout.settings_activity, null);

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
        ViewGroup fragmentContainer = settingsView.findViewById(R.id.content);
        mToolbar = settingsView.findViewById(R.id.action_bar);

        // Apply semantic colors to the top-level container and app bar.
        int backgroundColor = SemanticColorUtils.getSettingsBackgroundColor(mActivity);
        settingsView.findViewById(R.id.content).setBackgroundColor(backgroundColor);
        AppBarLayout appBarLayout = settingsView.findViewById(R.id.app_bar_layout);
        appBarLayout.setBackgroundColor(backgroundColor);
        appBarLayout.setElevation(0);
        appBarLayout.setStateListAnimator(null);

        // Set up the back navigation arrow in the toolbar.
        // TODO(crbug.com/521895796): This is a placeholder for testing. Move the arrow to
        // the right column before launch.
        mToolbar.setNavigationIcon(R.drawable.ic_arrow_back_24dp);
        mToolbar.setNavigationOnClickListener(v -> mActivity.onBackPressed());

        mToolbar.setTitle(R.string.settings);

        // Set up Help Menu on Toolbar.
        SettingsMenuHelper.onCreateOptionsMenu(mToolbar.getMenu(), mActivity);
        SettingsMenuHelper.onPrepareOptionsMenu(mToolbar.getMenu());
        mToolbar.setOnMenuItemClickListener(
                item -> SettingsMenuHelper.onOptionsItemSelected(item, mActivity, this));

        mSettingsHostFragment =
                (SettingsHostFragment) fragmentManager.findFragmentByTag(SETTINGS_NATIVE_PAGE_TAG);
        if (mSettingsHostFragment == null) {
            mSettingsHostFragment = new SettingsHostFragment();
            fragmentManager
                    .beginTransaction()
                    .add(fragmentContainer.getId(), mSettingsHostFragment, SETTINGS_NATIVE_PAGE_TAG)
                    .commitAllowingStateLoss();
        }
    }

    @Override
    public void destroySettings() {
        FragmentManager fragmentManager =
                ((FragmentActivity) mActivity).getSupportFragmentManager();
        assumeNonNull(mDependencyProvider);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mDependencyProvider);
        mDependencyProvider = null;
        mContainmentHelper.unregisterCallbacks(fragmentManager);
        assumeNonNull(mSettingsHostFragment);

        assumeNonNull(mTitleUpdaterLifecycleCallbacks);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mTitleUpdaterLifecycleCallbacks);
        mTitleUpdaterLifecycleCallbacks = null;

        assumeNonNull(mWideDisplayPaddingApplier);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mWideDisplayPaddingApplier);
        mWideDisplayPaddingApplier = null;

        assumeNonNull(mSettingsMetricsReporter);
        fragmentManager.unregisterFragmentLifecycleCallbacks(mSettingsMetricsReporter);
        mSettingsMetricsReporter = null;

        if (mMultiColumnTitleUpdater != null) {
            MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
            assumeNonNull(multiColumnSettings);
            multiColumnSettings.removeObserver(mMultiColumnTitleUpdater);
            mMultiColumnTitleUpdater = null;
        }

        if (mSearchCoordinator != null) {
            MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
            assumeNonNull(multiColumnSettings);
            multiColumnSettings.removeObserver(mSearchCoordinator);
            mSearchCoordinator.destroy();
            mSearchCoordinator = null;
        }

        if (mComponentCallbacks != null) {
            mActivity.unregisterComponentCallbacks(mComponentCallbacks);
            mComponentCallbacks = null;
        }

        fragmentManager.beginTransaction().remove(mSettingsHostFragment).commitAllowingStateLoss();
        mSettingsHostFragment = null;
        mToolbar = null;
    }

    private void createMultiColumnTitleUpdater(MultiColumnSettings multiColumnSettings, View view) {
        assert mMultiColumnTitleUpdater == null;

        LinearLayout titleContainer = view.findViewById(R.id.settings_title_in_detailed_pane);
        assumeNonNull(titleContainer);
        assumeNonNull(mToolbar);

        // TODO(crbug.com/521895796): Use proper fragment saved state.
        mMultiColumnTitleUpdater =
                new MultiColumnTitleUpdater(
                        /* savedInstanceState= */ null,
                        multiColumnSettings,
                        mActivity,
                        titleContainer,
                        mToolbar::setTitle,
                        this::onTitleTapped,
                        /* initialBreadcrumbPath= */ null);
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
        return HelpAndFeedbackLauncherImpl.getForProfile(mProfile);
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
        // TODO(crbug.com/521895796): Define settings-in-tab finish/back behavior.
    }

    @Override
    public boolean isTwoColumnSettingsVisible() {
        MultiColumnSettings multiColumnSettings = getMultiColumnSettings();
        return multiColumnSettings != null && multiColumnSettings.isTwoColumn();
    }

    @Override
    public PreferenceUpdateObserver getPreferenceUpdateObserver() {
        return this;
    }

    @Override
    public void onPreferencesUpdated(PreferenceFragmentCompat fragment) {
        mContainmentHelper.postUpdateContainmentOnLayout(fragment);
    }

    private void createSearchCoordinator(MultiColumnSettings multiColumnSettings, View view) {
        assert mSearchCoordinator == null;

        mSearchCoordinator =
                new SettingsSearchCoordinator(
                        (FragmentActivity) mActivity,
                        view.findViewById(R.id.action_bar),
                        this::isTwoColumnSettingsVisible,
                        multiColumnSettings,
                        mContainmentHelper.getItemDecorations(),
                        mProfile,
                        this::updateFirstVisibleTitle,
                        mModalDialogSupplier);

        multiColumnSettings.setOnCreateViewRunnable(
                () -> assumeNonNull(mSearchCoordinator).initializeSearchUi(null));
        multiColumnSettings.addObserver(mSearchCoordinator);
    }

    private void updateFirstVisibleTitle(int index) {
        assumeNonNull(mMultiColumnTitleUpdater).setFirstVisibleTitleIndex(index);
    }

    /** Utility class to handle creating the title updater. */
    private class TitleUpdaterLifecycleCallbacks
            extends FragmentManager.FragmentLifecycleCallbacks {
        @Override
        public void onFragmentViewCreated(
                FragmentManager fm, Fragment f, View v, @Nullable Bundle savedFragmentState) {
            if (f instanceof MultiColumnSettings multiColumnSettings) {
                createMultiColumnTitleUpdater(multiColumnSettings, v);
                createSearchCoordinator(multiColumnSettings, v);
            }
        }
    }
}
