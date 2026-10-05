// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup.PreferencePositionCallback;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helper class to manage containment styling and preference highlighting for settings fragments.
 */
@NullMarked
public class SettingsContainmentHelper {
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

    // Information of the view to highlight.
    private static class HighlightInfo {
        public final View view;
        public final HighlightParams params;

        private HighlightInfo(View view, HighlightParams params) {
            this.view = view;
            this.params = params;
        }
    }

    private final Context mContext;
    private final Delegate mDelegate;
    private final Map<PreferenceFragmentCompat, ContainmentItemDecoration> mItemDecorations =
            new HashMap<>();
    private final Map<PreferenceFragmentCompat, ViewTreeObserver.OnGlobalLayoutListener>
            mGlobalLayoutListeners = new HashMap<>();
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private FragmentManager.@Nullable FragmentLifecycleCallbacks mCallbacks;
    private @Nullable Runnable mRemoveResultChildViewListener;
    private @Nullable Runnable mTurnOffHighlight;
    private @Nullable ContainmentItemController mContainmentController;

    /**
     * Fragments that have had the settings theme applied to them. Used as a performance
     * optimization to avoid re-inflating view that have already been themed.
     */
    private final Set<PreferenceFragmentCompat> mThemedFragments = new HashSet<>();

    public SettingsContainmentHelper(Context context, Delegate delegate) {
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
                        if (f instanceof PreferenceFragmentCompat pf) {
                            Bundle args = pf.getArguments();
                            String highlightKey =
                                    args != null
                                            ? args.getString(
                                                    SettingsNavigation.EXTRA_HIGHLIGHT_PREFERENCE)
                                            : null;
                            if (!TextUtils.isEmpty(highlightKey)) {
                                args.remove(SettingsNavigation.EXTRA_HIGHLIGHT_PREFERENCE);
                                mHandler.post(
                                        () ->
                                                scrollAndHighlightItem(
                                                        pf,
                                                        highlightKey,
                                                        /* highlightKey= */ null,
                                                        /* subViewPos= */ 0));
                            }
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
        destroy();
    }

    /** Cleans up pending highlight callbacks and controllers. */
    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
        turnOffHighlight();
        mRemoveResultChildViewListener = null;
        mContainmentController = null;
    }

    /** Turns off the active preference highlight if one is showing. */
    public void turnOffHighlight() {
        if (mTurnOffHighlight != null) {
            mTurnOffHighlight.run();
            mTurnOffHighlight = null;
        }
    }

    /**
     * Scrolls to the given preference in {@code fragment} and highlights its view.
     *
     * @param fragment The {@link PreferenceFragmentCompat} containing the preference.
     * @param entryKey The key of the preference entry to scroll to and highlight.
     * @param highlightKey Optional key of a sub-preference to highlight instead of {@code
     *     entryKey}.
     * @param subViewPos Zero-based index of the styled sub-view to highlight when {@code
     *     highlightKey} is non-null.
     */
    public void scrollAndHighlightItem(
            PreferenceFragmentCompat fragment,
            String entryKey,
            @Nullable String highlightKey,
            int subViewPos) {
        if (fragment.getView() == null) return;
        RecyclerView listView = fragment.getListView();
        if (listView == null) return;
        assert listView.getAdapter() instanceof PreferencePositionCallback
                : "Recycler adapter must implement PreferencePositionCallback";
        var listAdapter = (PreferencePositionCallback) listView.getAdapter();
        boolean highlightSubView = highlightKey != null;
        String key = assumeNonNull(highlightSubView ? highlightKey : entryKey);

        // Zero-based position of the preference view in listView.
        int pos = listAdapter.getPreferenceAdapterPosition(key);
        if (pos < 0) {
            // Fragment that builds preferences dynamically (not with an xml resource but using
            // APIs) is not ready to return the right position of the item to highlight and scroll
            // to, even though the associated view would already have been attached. Take a
            // different approach to do the scrolling and highlighting i.e. wait a few more
            // layout passes for the view holder to be available.
            mHandler.post(
                    () ->
                            scrollAndHighlightDynamicPref(
                                    fragment, key, highlightSubView, subViewPos));
            return;
        }
        mRemoveResultChildViewListener = null;
        var attachListener =
                new RecyclerView.OnChildAttachStateChangeListener() {
                    @Override
                    public void onChildViewAttachedToWindow(View view) {
                        // |attach| events for a preference view may be invoked multiple times,
                        // intertwined with |detach| in close succession. We should use the last
                        // event to highlight the corresponding preference view. The listener
                        // is removed after that.
                        if (fragment.getView() == null || fragment.getListView() == null) return;
                        var viewHolder = fragment.getListView().getChildViewHolder(view);
                        if (viewHolder != null && pos == viewHolder.getBindingAdapterPosition()) {
                            scheduleHighlight(
                                    listView,
                                    this,
                                    fragment,
                                    view,
                                    pos,
                                    highlightSubView,
                                    subViewPos);
                        }
                    }

                    @Override
                    public void onChildViewDetachedFromWindow(View view) {}
                };
        listView.addOnChildAttachStateChangeListener(attachListener);
        var existingHolder = listView.findViewHolderForAdapterPosition(pos);
        if (existingHolder != null) {
            scheduleHighlight(
                    listView,
                    attachListener,
                    fragment,
                    existingHolder.itemView,
                    pos,
                    highlightSubView,
                    subViewPos);
        }
        scrollToPref(fragment, key);
    }

    private void scheduleHighlight(
            RecyclerView listView,
            RecyclerView.OnChildAttachStateChangeListener listener,
            PreferenceFragmentCompat fragment,
            View view,
            int pos,
            boolean highlightSubView,
            int subViewPos) {
        if (mRemoveResultChildViewListener != null) {
            mHandler.removeCallbacks(mRemoveResultChildViewListener);
        }
        mRemoveResultChildViewListener =
                () -> {
                    mRemoveResultChildViewListener = null;
                    listView.removeOnChildAttachStateChangeListener(listener);
                    if (fragment.getView() == null) return;
                    highlightItem(fragment, view, pos, highlightSubView, subViewPos);
                };
        mHandler.postDelayed(mRemoveResultChildViewListener, 200);
    }

    private void scrollAndHighlightDynamicPref(
            PreferenceFragmentCompat fragment,
            String key,
            boolean highlightSubView,
            int subViewPos) {
        if (fragment.getView() == null) return;
        RecyclerView listView = fragment.getListView();
        if (listView == null) return;

        var listAdapter = (PreferencePositionCallback) listView.getAdapter();
        int pos = assumeNonNull(listAdapter).getPreferenceAdapterPosition(key);
        var viewHolder = listView.findViewHolderForAdapterPosition(pos);
        if (viewHolder == null) {
            mHandler.post(
                    () ->
                            scrollAndHighlightDynamicPref(
                                    fragment, key, highlightSubView, subViewPos));
        } else {
            highlightItem(fragment, viewHolder.itemView, pos, highlightSubView, subViewPos);
            scrollToPref(fragment, key);
        }
    }

    private void highlightItem(
            PreferenceFragmentCompat fragment,
            View view,
            int pos,
            boolean highlightSubView,
            int viewPos) {
        var info = getHighlightInfo(fragment, view, pos, highlightSubView, viewPos);
        ViewHighlighter.turnOnHighlight(info.view, info.params);
        mHandler.post(
                () -> {
                    mTurnOffHighlight = () -> ViewHighlighter.turnOffHighlight(info.view);
                });
    }

    private void scrollToPref(PreferenceFragmentCompat fragment, String key) {
        RecyclerView listView = fragment.getListView();
        boolean containmentStyleDisabled =
                mItemDecorations.isEmpty() && mGlobalLayoutListeners.isEmpty();
        if (containmentStyleDisabled) {
            fragment.scrollToPreference(key);
        } else {
            // Calling #scrollToPreference directly doesn't work when if containment styled is
            // enabled. But OnScrollListener#onScrolled is always invoked after the recycler view
            // layout pass is completed. Use this timing to actually scroll the fragment to
            // the chosen preference.
            listView.addOnScrollListener(
                    new RecyclerView.OnScrollListener() {
                        @Override
                        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {}

                        @Override
                        public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                            fragment.scrollToPreference(key);
                            listView.removeOnScrollListener(this);
                        }
                    });
        }
        listView.addOnItemTouchListener(
                new RecyclerView.SimpleOnItemTouchListener() {
                    @Override
                    public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent e) {
                        if (mTurnOffHighlight != null) {
                            mTurnOffHighlight.run();
                            mTurnOffHighlight = null;
                            listView.removeOnItemTouchListener(this);
                        }
                        return false;
                    }
                });
    }

    private HighlightInfo getHighlightInfo(
            PreferenceFragmentCompat fragment,
            View view,
            int pos,
            boolean highlightSubView,
            int subViewPos) {
        var params = new HighlightParams(HighlightShape.RECTANGLE);
        var defaultRes = new HighlightInfo(view, params);
        if (highlightSubView) {
            List<View> views = new ArrayList<>();
            ContainmentViewStyler.recursivelyFindStyledViews(view, views);
            if (views.isEmpty() || subViewPos >= views.size()) return defaultRes;

            if (mContainmentController == null) {
                mContainmentController = new ContainmentItemController(mContext);
            }
            var style = mContainmentController.generateViewStyles(views).get(subViewPos);
            params.setTopCornerRadius((int) style.getTopRadius());
            params.setBottomCornerRadius((int) style.getBottomRadius());
            return new HighlightInfo(views.get(subViewPos), params);
        } else {
            var itemDecoration = mItemDecorations.get(fragment);
            if (itemDecoration == null) return defaultRes;

            var style = itemDecoration.getContainerStyle(pos);
            if (style == null) return defaultRes;

            params.setTopCornerRadius((int) style.getTopRadius());
            params.setBottomCornerRadius((int) style.getBottomRadius());
            return defaultRes;
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
        if (mDelegate.isTwoColumnSettingsVisible()) {
            controller.setHorizontalMargin(0);
        }
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
}
