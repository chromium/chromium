// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Helper class to manage containment styling for settings fragments. */
@NullMarked
class SettingsContainmentHelper {
    /**
     * Delegate interface implemented by the user of this helper. Allows access to data owned by
     * {@link SettingsActivity} or {@link SettingsPageFragmentDelegateImpl} respectively.
     */
    public interface Delegate {
        /** Returns whether two column settings is visible (because the window is wide enough). */
        boolean isTwoColumnSettingsVisible();

        /** Returns the MultiColumnSettings for the user of this helper. */
        @Nullable MultiColumnSettings getMultiColumnSettings();

        /**
         * Returns the PreferenceUpdateObserver for the user of this helper, usually the class
         * itself.
         */
        PreferenceUpdateObserver getPreferenceUpdateObserver();
    }

    private final Context mContext;
    private final Delegate mDelegate;
    private final Map<PreferenceFragmentCompat, ContainmentItemDecoration> mItemDecorations =
            new HashMap<>();
    private final Map<PreferenceFragmentCompat, ViewTreeObserver.OnGlobalLayoutListener>
            mGlobalLayoutListeners = new HashMap<>();
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mCallbacks;

    /**
     * Fragments that have had the settings theme applied to them. Used as a performance
     * optimization to avoid re-inflating view that have already been themed.
     */
    private final Set<PreferenceFragmentCompat> mThemedFragments = new HashSet<>();

    SettingsContainmentHelper(Context context, Delegate delegate) {
        mContext = context;
        mDelegate = delegate;
    }

    /**
     * Registers the fragment lifecycle callbacks for containment styling.
     *
     * @param fragmentManager The FragmentManager to register the callbacks on.
     */
    void registerCallbacks(FragmentManager fragmentManager) {
        assert mCallbacks == null : "Callbacks already registered";
        mCallbacks =
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentAttached(
                            FragmentManager fm, Fragment f, Context context) {
                        if (f instanceof PreferenceUpdateObserver.Provider provider) {
                            provider.setPreferenceUpdateObserver(
                                    mDelegate.getPreferenceUpdateObserver());
                        }
                    }

                    @Override
                    public void onFragmentDetached(FragmentManager fm, Fragment f) {
                        if (f instanceof PreferenceUpdateObserver.Provider provider) {
                            provider.removePreferenceUpdateObserver();
                        }
                    }

                    @Override
                    public void onFragmentViewCreated(
                            FragmentManager fm,
                            Fragment fragment,
                            View v,
                            @Nullable Bundle savedInstanceState) {
                        if (fragment instanceof PreferenceFragmentCompat preferenceFragment) {
                            postUpdateContainmentOnLayout(preferenceFragment);
                        }
                    }

                    @Override
                    public void onFragmentViewDestroyed(FragmentManager fm, Fragment f) {
                        if (f instanceof PreferenceFragmentCompat preferenceFragmentCompat) {
                            SettingsContainmentHelper.this.onFragmentViewDestroyed(
                                    preferenceFragmentCompat);
                        }
                    }
                };
        fragmentManager.registerFragmentLifecycleCallbacks(mCallbacks, /* recursive= */ true);
    }

    /**
     * Unregisters the fragment lifecycle callbacks.
     *
     * @param fragmentManager The FragmentManager to unregister the callbacks from.
     */
    void unregisterCallbacks(FragmentManager fragmentManager) {
        if (mCallbacks != null) {
            fragmentManager.unregisterFragmentLifecycleCallbacks(mCallbacks);
            mCallbacks = null;
        }
    }

    /**
     * Helper method to update containment UI on layout completion.
     *
     * <p>TODO(crbug.com/439911511): Improve Javadoc.
     */
    void postUpdateContainmentOnLayout(PreferenceFragmentCompat fragment) {
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

    /**
     * Updates containment styling for all attached fragments recursively in the given
     * FragmentManager.
     *
     * @param fragmentManager The FragmentManager containing the fragments to update.
     */
    void updateContainmentForAttachedFragments(FragmentManager fragmentManager) {
        for (Fragment fragment : fragmentManager.getFragments()) {
            if (fragment != null && fragment.isAdded()) {
                if (fragment instanceof PreferenceFragmentCompat preferenceFragment) {
                    updateFragmentContainment(preferenceFragment);
                }
                updateContainmentForAttachedFragments(fragment.getChildFragmentManager());
            }
        }
    }

    /**
     * Applies or removes containment styling for fragments within the multi-column settings layout
     * based on whether the multi-column layout is currently active.
     *
     * <p>TODO(crbug.com/439911511): Improve Javadoc.
     */
    void updateFragmentContainment(PreferenceFragmentCompat fragment) {
        if (fragment == null) {
            return;
        }

        if (mDelegate.isTwoColumnSettingsVisible()
                && fragment instanceof MainSettings mainSettingsFragment) {
            applyMainSettingsFragmentDecoration(mainSettingsFragment);
        } else {
            applyContainmentForFragment(fragment);
        }
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

        // Use getContext() instead of requireContext() for mocking in tests.
        Context context = fragment.getContext();
        if (context == null) return;

        if (mThemedFragments.add(fragment)) {
            context.getTheme().applyStyle(R.style.ThemeOverlay_Chromium_Settings_Containment, true);
        }

        final var recyclerView = fragment.getListView();
        if (recyclerView == null) return;

        ContainmentItemController controller = new ContainmentItemController(mContext);
        if (mDelegate.isTwoColumnSettingsVisible()) controller.setHorizontalMargin(0);
        ContainmentItemDecoration itemDecoration = mItemDecorations.get(fragment);
        if (itemDecoration == null) {
            itemDecoration = new ContainmentItemDecoration(controller);
            mItemDecorations.put(fragment, itemDecoration);
            recyclerView.addItemDecoration(itemDecoration);
            // Force a re-inflation of all views to ensure they pick up the new theme.
            // This is only needed the first time the theme is applied to this fragment view.
            reInflateViews(fragment);
        }
        itemDecoration.updatePreferenceStyles(
                controller.generatePreferenceStyles(
                        SettingsUtils.getVisiblePreferences(fragment.getPreferenceScreen())));
        recyclerView.invalidateItemDecorations();
    }

    private void applyMainSettingsFragmentDecoration(MainSettings mainSettings) {
        // Use getContext() instead of requireContext() for mocking in tests.
        Context context = mainSettings.getContext();
        if (context == null) return;

        // Ensure any ContainmentItemDecoration previously added to MainSettings (e.g. during
        // single-column mode or initial layout pass) is removed when entering two-column mode.
        ContainmentItemDecoration itemDecoration = mItemDecorations.remove(mainSettings);
        if (itemDecoration != null && mainSettings.getListView() != null) {
            mainSettings.getListView().removeItemDecoration(itemDecoration);
        }

        if (mThemedFragments.add(mainSettings)) {
            context.getTheme().applyStyle(R.style.ThemeOverlay_Chromium_Settings_Containment, true);
            reInflateViews(mainSettings);
        }

        int verticalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_container_vertical_margin);
        int leftMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.settings_item_margin);
        float radius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_rounded_corner_radius_default);
        int selectedBackgroundColor =
                SemanticColorUtils.getSettingsMainMenuSelectedBackgroundColor(context);
        // TODO(crbug.com/439911511): `SelectionDecoration`'s name does not fully capture its
        // current responsibility, which inadvertently includes handling decoration removal
        // for `MainSettings` when in two-column mode. Consider renaming it to reflect this broader
        // role.
        mainSettings.setMultiColumnSettings(
                mDelegate.getMultiColumnSettings(),
                new SelectionDecoration(
                        verticalMargin, leftMargin, radius, selectedBackgroundColor));
    }

    private void reInflateViews(PreferenceFragmentCompat fragment) {
        if (fragment.getListView() == null) return;

        var adapter = fragment.getListView().getAdapter();
        fragment.getListView().setAdapter(null);
        fragment.getListView().setAdapter(adapter);
    }

    void onFragmentViewDestroyed(PreferenceFragmentCompat fragment) {
        mThemedFragments.remove(fragment);
        mItemDecorations.remove(fragment);
        ViewTreeObserver.OnGlobalLayoutListener listener = mGlobalLayoutListeners.remove(fragment);
        View view = fragment.getView();
        if (listener != null && view != null) {
            view.getViewTreeObserver().removeOnGlobalLayoutListener(listener);
        }
    }

    Map<PreferenceFragmentCompat, ContainmentItemDecoration> getItemDecorations() {
        return mItemDecorations;
    }
}
