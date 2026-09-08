// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/** Hosts settings preference fragments inside a native page. See {@link SettingsPage}. */
@NullMarked
public class SettingsHostFragment extends Fragment
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback,
                PreferenceUpdateObserver,
                SettingsContainmentHelper.Delegate {

    public static final String SETTINGS_NATIVE_PAGE_TAG = "settings_native_page";

    private static final int CONTAINER_ID = View.generateViewId();

    private @Nullable Context mThemedContext;
    private @Nullable WeakReference<Fragment> mFinishedMainFragment;
    private @Nullable FragmentDependencyProvider mDependencyProvider;
    private @Nullable SettingsContainmentHelper mContainmentHelper;
    private @Nullable WideDisplayPaddingApplier mWideDisplayPaddingApplier;
    private @Nullable SettingsNavigation mSettingsNavigation;
    private @Nullable String mInitialUrl;
    private @Nullable Bundle mSavedInstanceState;
    private @Nullable Callback<Bundle> mSaveInstanceStateCallback;
    private int mPendingPopBackCount;

    /** Public constructor needed for Fragment re-instantiation. */
    public SettingsHostFragment() {
        assert SettingsInTab.isEnabled()
                : "SettingsInTab feature must be enabled to use SettingsHostFragment.";
    }

    /** Sets the dependency provider for child fragments. */
    public void setDependencyProvider(FragmentDependencyProvider dependencyProvider) {
        if (mDependencyProvider != null && isAdded()) {
            getChildFragmentManager().unregisterFragmentLifecycleCallbacks(mDependencyProvider);
        }
        mDependencyProvider = dependencyProvider;
        if (isAdded()) {
            // Register callbacks to add the dependency provider to future child fragments.
            getChildFragmentManager()
                    .registerFragmentLifecycleCallbacks(mDependencyProvider, /* recursive= */ true);

            // Ensure existing child fragments have dependencies attached. This is necessary when
            // the activity restarts, for example after a theme change.
            attachDependenciesRecursively(getChildFragmentManager());
        }
    }

    /** Attaches the {@link #mDependencyProvider} to child fragments recursively. */
    private void attachDependenciesRecursively(FragmentManager fragmentManager) {
        assert mDependencyProvider != null;
        for (Fragment fragment : fragmentManager.getFragments()) {
            if (fragment != null && fragment.isAdded()) {
                mDependencyProvider.attachDependencies(fragmentManager, fragment);
                attachDependenciesRecursively(fragment.getChildFragmentManager());
            }
        }
    }

    @Override
    public boolean isTwoColumnSettingsVisible() {
        Fragment active = getActiveFragment();
        return active instanceof MultiColumnSettings multiColumnSettings
                && multiColumnSettings.isTwoColumn();
    }

    @Override
    public @Nullable MultiColumnSettings getMultiColumnSettings() {
        Fragment active = getActiveFragment();
        return active instanceof MultiColumnSettings multiColumnSettings
                ? multiColumnSettings
                : null;
    }

    @Override
    public PreferenceUpdateObserver getPreferenceUpdateObserver() {
        return this;
    }

    @Override
    public void onPreferencesUpdated(PreferenceFragmentCompat fragment) {
        if (mContainmentHelper != null) {
            mContainmentHelper.postUpdateContainmentOnLayout(fragment);
        }
    }

    public @Nullable SettingsContainmentHelper getContainmentHelper() {
        return mContainmentHelper;
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mPendingPopBackCount == 0) return;

        // If we have pending back entries, show the correct fragment. This can happen when a
        // fragment called finishCurrentSettings() and needs to pop the back stack to show the
        // correct fragment.
        Fragment activeFragment = getActiveFragment();
        FragmentManager fragmentManager =
                activeFragment instanceof MultiColumnSettings multiColumnSettings
                        ? multiColumnSettings.getChildFragmentManager()
                        : getChildFragmentManager();
        if (fragmentManager.getBackStackEntryCount() <= mPendingPopBackCount) {
            // Show the main settings UI (which is represented by null).
            showFragment(null, /* addToBackStack= */ false, /* tag= */ null);
        } else {
            var backStackEntry =
                    fragmentManager.getBackStackEntryAt(
                            fragmentManager.getBackStackEntryCount() - mPendingPopBackCount);
            fragmentManager.popBackStack(
                    backStackEntry.getId(), FragmentManager.POP_BACK_STACK_INCLUSIVE);
        }
        mPendingPopBackCount = 0;
    }

    @Override
    public void onAttach(Context context) {
        // Ensure child fragments inherit the same Chromium Settings theme used by SettingsActivity.
        // For example, this ensures the left column category labels are styled correctly.
        mThemedContext = new ContextThemeWrapper(context, R.style.ThemeOverlay_Chromium_Settings);
        super.onAttach(mThemedContext);

        mContainmentHelper = new SettingsContainmentHelper(mThemedContext, this);
        mContainmentHelper.registerCallbacks(getChildFragmentManager());

        // Ensure wide display padding is applied and dividers are removed from child fragments
        // (e.g. during activity restart after dark/light theme changes).
        mWideDisplayPaddingApplier =
                new WideDisplayPaddingApplier(
                        mThemedContext,
                        this::isTwoColumnSettingsVisible,
                        /* mainFragmentTag= */ null);
        getChildFragmentManager()
                .registerFragmentLifecycleCallbacks(
                        mWideDisplayPaddingApplier, /* recursive= */ true);

        // Optionally create a temporary dependency provider for the current activity. This is only
        // called when the fragment is attached to an activity and the dependency provider has not
        // been set yet, for example during dark/light theme changes.
        if (mDependencyProvider == null && ProfileManager.isInitialized()) {
            mDependencyProvider = createOnAttachDependencyProvider();
        }

        // Either way, ensure fragment lifecycle callbacks are set.
        if (mDependencyProvider != null) {
            getChildFragmentManager()
                    .registerFragmentLifecycleCallbacks(mDependencyProvider, /* recursive= */ true);
            attachDependenciesRecursively(getChildFragmentManager());
        }
    }

    @Override
    public void onDetach() {
        if (mWideDisplayPaddingApplier != null) {
            getChildFragmentManager()
                    .unregisterFragmentLifecycleCallbacks(mWideDisplayPaddingApplier);
            mWideDisplayPaddingApplier = null;
        }
        if (mContainmentHelper != null) {
            mContainmentHelper.unregisterCallbacks(getChildFragmentManager());
            mContainmentHelper = null;
        }
        super.onDetach();
    }

    /**
     * Creates a temporary {@link FragmentDependencyProvider} for the current activity.
     *
     * <p>This is called when the fragment is attached to an activity during state restoration (e.g.
     * theme change) before {@link #setDependencyProvider} is called by {@link
     * SettingsPageFragmentDelegateImpl}.
     */
    private FragmentDependencyProvider createOnAttachDependencyProvider() {
        assert ProfileManager.isInitialized();
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        OneshotSupplierImpl<WindowAndroid> windowAndroidSupplier = new OneshotSupplierImpl<>();
        OneshotSupplierImpl<SnackbarManager> snackbarSupplier = new OneshotSupplierImpl<>();
        OneshotSupplierImpl<BottomSheetController> bottomSheetSupplier =
                new OneshotSupplierImpl<>();
        Activity activity = requireActivity();
        assert activity instanceof ChromeBaseAppCompatActivity;
        ChromeBaseAppCompatActivity chromeActivity = (ChromeBaseAppCompatActivity) activity;
        ActivityResultTracker activityResultTracker = chromeActivity.getActivityResultTracker();
        MonotonicObservableSupplier<ModalDialogManager> modalDialogSupplier =
                chromeActivity.getModalDialogManagerSupplier();

        return new FragmentDependencyProvider(
                activity,
                profile,
                windowAndroidSupplier,
                activityResultTracker,
                snackbarSupplier,
                bottomSheetSupplier,
                modalDialogSupplier,
                SupplierUtils.ofNull());
    }

    @Override
    public Context getContext() {
        return mThemedContext != null ? mThemedContext : assumeNonNull(super.getContext());
    }

    @Override
    public LayoutInflater onGetLayoutInflater(@Nullable Bundle savedInstanceState) {
        LayoutInflater inflater = super.onGetLayoutInflater(savedInstanceState);
        // Ensure we use the themed context if available.
        return inflater.cloneInContext(getContext());
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mSavedInstanceState = savedInstanceState;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // Save per-tab settings state (e.g. search coordinator, title updater, breadcrumbs) in
        // this host fragment's bundle so multiple settings tabs don't collide in the Activity's
        // shared saved instance state during Activity recreation (such as theme changes).
        if (mSaveInstanceStateCallback != null) {
            mSaveInstanceStateCallback.onResult(outState);
        }
    }

    /** Returns the saved instance state bundle for this fragment. */
    public @Nullable Bundle getSavedInstanceState() {
        return mSavedInstanceState;
    }

    /** Sets the callback invoked when this host fragment saves its instance state. */
    public void setSaveInstanceStateCallback(@Nullable Callback<Bundle> callback) {
        mSaveInstanceStateCallback = callback;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        FrameLayout frameLayout = new FrameLayout(requireContext());
        frameLayout.setId(CONTAINER_ID);
        return frameLayout;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (savedInstanceState == null) {
            Fragment initialFragment = createInitialFragment(requireActivity().getIntent());
            getChildFragmentManager()
                    .beginTransaction()
                    .add(CONTAINER_ID, initialFragment)
                    .commitNowAllowingStateLoss();
        }
    }

    /** Sets the initial settings URL for tab-based navigation. */
    public void setInitialUrl(@Nullable String initialUrl) {
        mInitialUrl = initialUrl;
    }

    /** Returns the initial settings URL for tab-based navigation. */
    public @Nullable String getInitialUrl() {
        return mInitialUrl;
    }

    /**
     * Clears stored initial URL once consumed or superseded by explicit in-tab URL navigation,
     * ensuring subsequent resets to the root chrome://settings page load default Account fragment
     * instead of reusing an old subpage URL.
     */
    public void clearInitialUrl() {
        mInitialUrl = null;
        Fragment activeFragment = getActiveFragment();
        if (activeFragment instanceof MultiColumnSettings multiColumnSettings) {
            multiColumnSettings.setInitialUrl(null);
        }
    }

    /**
     * Creates the initial fragment to be shown in the settings page. Allows overrides for testing
     * to use simpler fragments.
     *
     * @param intent The intent to use to create the initial settings detail page. If null, shows
     *     the default settings page.
     */
    protected Fragment createInitialFragment(@Nullable Intent intent) {
        var multiColumnSettings = new MultiColumnSettings();
        if (intent != null) {
            multiColumnSettings.setPendingFragmentIntent(intent);
        }
        if (mInitialUrl != null) {
            multiColumnSettings.setInitialUrl(mInitialUrl);
            mInitialUrl = null;
        }
        return multiColumnSettings;
    }

    /** Sets the tab-scoped {@link SettingsNavigation} delegate for this host fragment. */
    public void setSettingsNavigation(@Nullable SettingsNavigation settingsNavigation) {
        mSettingsNavigation = settingsNavigation;
    }

    /** Returns the tab-scoped {@link SettingsNavigation} delegate bound to this host fragment. */
    public @Nullable SettingsNavigation getSettingsNavigation() {
        return mSettingsNavigation;
    }

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        String fragmentClassName = preference.getFragment();
        if (fragmentClassName == null) return false;

        // When SettingsInTab URL navigation is enabled, delegate subfragment launches through
        // the tab-scoped SettingsNavigation instance bound to this fragment. This translates
        // the target fragment into a canonical URL and loads it via Tab.loadUrl(), pushing a
        // new NavigationEntry to WebContents history, updating the Omnibox URL, and
        // integrating with browser Back/Forward navigation stack.
        if (ChromeFeatureList.sSettingsInTabUrlNav.isEnabled() && mSettingsNavigation != null) {
            try {
                var fragmentClass = Class.forName(fragmentClassName).asSubclass(Fragment.class);
                mSettingsNavigation.startSettings(
                        requireContext(), fragmentClass, preference.getExtras());
                return true;
            } catch (ClassNotFoundException e) {
                // Fall back to direct showFragment if class loading fails.
            }
        }

        Fragment fragment =
                Fragment.instantiate(requireContext(), fragmentClassName, preference.getExtras());
        return showFragment(fragment, /* addToBackStack= */ true, /* tag= */ null);
    }

    /**
     * Returns whether the fragment is attached to an activity. This method can be mocked in tests,
     * unlike Fragment#isAdded(), which is final.
     */
    boolean isAttachedToActivity() {
        return isAdded();
    }

    /** Returns the currently active fragment hosted by this fragment. */
    public @Nullable Fragment getActiveFragment() {
        return getChildFragmentManager().findFragmentById(CONTAINER_ID);
    }

    /** Returns whether the given fragment is a direct child of this host fragment. */
    public boolean containsChild(Fragment fragment) {
        if (!isAdded()) return false;

        for (Fragment f : getChildFragmentManager().getFragments()) {
            if (f == fragment) return true;
        }
        return false;
    }

    /**
     * Returns the active {@link SettingsHostFragment} if attached to the activity and shown, or
     * null.
     */
    public static @Nullable SettingsHostFragment get(@Nullable Activity activity) {
        if (!(activity instanceof FragmentActivity fragmentActivity)) return null;

        // Setting host fragments have unique IDs, so search for the first attached visible one.
        for (Fragment f : fragmentActivity.getSupportFragmentManager().getFragments()) {
            if (f instanceof SettingsHostFragment settingsHostFragment
                    && settingsHostFragment.isAttachedToActivity()) {
                if (activity instanceof SettingsActivityInterface
                        || (settingsHostFragment.getView() != null
                                && settingsHostFragment.getView().isShown())) {
                    return settingsHostFragment;
                }
            }
        }
        return null;
    }

    /**
     * Returns the enclosing {@link SettingsHostFragment} containing the given fragment, or null if
     * the fragment is not hosted by a SettingsHostFragment. Works even if the fragment is not shown
     * (e.g. Chrome is in the background).
     */
    public static @Nullable SettingsHostFragment get(@Nullable Fragment fragment) {
        Fragment current = fragment;
        while (current != null) {
            if (current instanceof SettingsHostFragment settingsHostFragment) {
                return settingsHostFragment;
            }
            current = current.getParentFragment();
        }
        return null;
    }

    /**
     * Shows a fragment inside the settings native page container or detail pane. Does nothing if
     * the settings tab is not open (and returns false).
     *
     * @param fragment The settings fragment to show. If null, the main settings page will show.
     * @param addToBackStack Whether to add the fragment to the back stack.
     * @param tag The tag to use for the fragment.
     */
    public boolean showFragment(
            @Nullable Fragment fragment, boolean addToBackStack, @Nullable String tag) {
        if (!isAttachedToActivity()) return false;

        Fragment activeFragment = getActiveFragment();
        if (activeFragment instanceof MultiColumnSettings multiColumnSettings) {
            if (fragment == null || fragment instanceof MainSettings) {
                var slidingPane = multiColumnSettings.getSlidingPaneLayoutOrNull();
                if (slidingPane != null && slidingPane.isSlideable()) {
                    slidingPane.closePane();
                }
                // Show the default detail fragment.
                Fragment initialFragment = multiColumnSettings.onCreateInitialDetailFragment();
                if (initialFragment != null) {
                    multiColumnSettings.showDetailFragment(
                            initialFragment, /* addToBackStack= */ false, /* tag= */ null);
                }
                return true;
            }
            multiColumnSettings.showDetailFragment(fragment, addToBackStack, tag);
            return true;
        }

        if (fragment == null) {
            fragment = createInitialFragment(/* intent= */ null);
        }

        var transaction = getChildFragmentManager().beginTransaction();
        transaction.replace(CONTAINER_ID, fragment);
        if (addToBackStack) {
            transaction.addToBackStack(tag);
        }
        transaction.commitAllowingStateLoss();
        return true;
    }

    /**
     * Returns the fragment showing as the settings main content, typically a {@link
     * PreferenceFragmentCompat}.
     */
    public @Nullable Fragment getMainFragment() {
        Fragment activeFragment = getActiveFragment();
        if (activeFragment instanceof MultiColumnSettings multiColumnSettings) {
            // In single-column mode when the detail pane is closed, the user is viewing the
            // top-level MainSettings header rather than the detail pane. Return MainSettings
            // instead of a possibly-stale detail fragment.
            if (!multiColumnSettings.isTwoColumn() && !multiColumnSettings.isLayoutOpen()) {
                return multiColumnSettings.getMainSettings();
            }
            return multiColumnSettings
                    .getChildFragmentManager()
                    .findFragmentById(R.id.preferences_detail);
        }
        return activeFragment;
    }

    /**
     * Finishes the current settings fragment. If the given fragment is not the current one, or the
     * fragment is already finished, this method does nothing. If the back stack is empty, shows the
     * main settings page.
     *
     * @param fragment The expected current fragment.
     */
    @SuppressLint("ReferenceEquality")
    public void finishCurrentSettings(Fragment fragment) {
        if (getMainFragment() != fragment) {
            return;
        }
        if (mFinishedMainFragment != null && mFinishedMainFragment.get() == fragment) {
            return;
        }
        mFinishedMainFragment = new WeakReference<>(fragment);

        Fragment activeFragment = getActiveFragment();
        FragmentManager fragmentManager =
                activeFragment instanceof MultiColumnSettings multiColumnSettings
                        ? multiColumnSettings.getChildFragmentManager()
                        : getChildFragmentManager();
        // Defer popping or navigating back until onStart() if fragment state has already been
        // saved,
        // preventing IllegalStateException from performing transactions while stopped or
        // backgrounded.
        if (fragmentManager.isStateSaved()) {
            ++mPendingPopBackCount;
        } else if (fragmentManager.getBackStackEntryCount() == 0) {
            // Show the main settings UI (which is represented by null).
            showFragment(null, /* addToBackStack= */ false, /* tag= */ null);
        } else {
            fragmentManager.popBackStack();
        }
    }

    /** Executes pending navigations immediately. */
    void executePendingNavigations() {
        getChildFragmentManager().executePendingTransactions();
        Fragment activeFragment = getActiveFragment();
        if (activeFragment instanceof MultiColumnSettings multiColumnSettings) {
            multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        }
    }

    /** Updates containment styling for all attached child fragments recursively. */
    public void updateContainmentForAttachedFragments() {
        if (isAttachedToActivity() && mContainmentHelper != null) {
            mContainmentHelper.updateContainmentForAttachedFragments(getChildFragmentManager());
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateContainmentForAttachedFragments();
    }

    /** Returns the number of entries in the child fragment manager back stack. */
    public int getBackStackEntryCount() {
        return isAdded() ? getChildFragmentManager().getBackStackEntryCount() : 0;
    }

    /** Pops the top entry from the child fragment manager back stack. */
    public void popBackStack() {
        assert isAdded();
        getChildFragmentManager().popBackStack();
    }

    void setContainmentHelperForTesting(SettingsContainmentHelper containmentHelper) {
        mContainmentHelper = containmentHelper;
    }

    public @Nullable FragmentDependencyProvider getDependencyProviderForTesting() {
        return mDependencyProvider;
    }
}
