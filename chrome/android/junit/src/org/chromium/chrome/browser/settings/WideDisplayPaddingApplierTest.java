// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.ui.base.ViewUtils;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;

/** Unit tests for {@link WideDisplayPaddingApplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WideDisplayPaddingApplierTest {
    private static final String MAIN_TAG = "main_fragment";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<FragmentActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(FragmentActivity.class);

    @Mock private BooleanSupplier mIsTwoColumnSettingsVisibleSupplier;

    private FragmentActivity mTestActivity;
    private WideDisplayPaddingApplier mApplier;

    /** A test PreferenceFragmentCompat subclass. */
    public static class TestSettingsFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            Context context = getPreferenceManager().getContext();
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(context);
            Preference preference = new Preference(context);
            preference.setTitle("Test preference");
            screen.addPreference(preference);
            setPreferenceScreen(screen);
        }
    }

    /** A simple Fragment subclass for testing non-PreferenceFragmentCompat fragments. */
    public static class TestFragment extends Fragment {
        public TestFragment() {}

        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            return new View(requireContext());
        }
    }

    /**
     * A settings page that is not a PreferenceFragmentCompat, like SearchEngineSettings or the
     * language and autofill editor pages.
     */
    public static class TestEmbeddablePageFragment extends Fragment
            implements EmbeddableSettingsPage {
        private final SettableMonotonicObservableSupplier<String> mPageTitle =
                ObservableSuppliers.createMonotonic();

        public TestEmbeddablePageFragment() {}

        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            FrameLayout root = new FrameLayout(requireContext());
            root.addView(new View(requireContext()));
            return root;
        }

        @Override
        public MonotonicObservableSupplier<String> getPageTitle() {
            return mPageTitle;
        }

        @Override
        public @SettingsFragment.AnimationType int getAnimationType() {
            return SettingsFragment.AnimationType.PROPERTY;
        }
    }

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mTestActivity = activity;
                        });
        mTestActivity.setTheme(R.style.Theme_Chromium_Settings);

        mApplier =
                new WideDisplayPaddingApplier(
                        mTestActivity, mIsTwoColumnSettingsVisibleSupplier, MAIN_TAG);
        mTestActivity
                .getSupportFragmentManager()
                .registerFragmentLifecycleCallbacks(mApplier, /* recursive= */ true);

        // Default to not two-column.
        when(mIsTwoColumnSettingsVisibleSupplier.getAsBoolean()).thenReturn(false);
    }

    @Test
    public void testPreferenceFragment_appliesPaddingOnViewCreated() {
        TestSettingsFragment fragment = new TestSettingsFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);
        RecyclerView recyclerView = view.findViewById(R.id.recycler_view);
        assertNotNull(recyclerView);

        // Padding decoration should be applied synchronously on view creation.
        assertTrue(hasPaddedItemDecoration(recyclerView));
    }

    @Test
    @Config(qualifiers = "sw320dp") // Start with narrow display
    public void testMainFragmentWithMatchingTag_appliesPadding() {
        TestFragment fragment = new TestFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, MAIN_TAG)
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);

        // Transition to wide.
        Configuration config = new Configuration(mTestActivity.getResources().getConfiguration());
        config.screenWidthDp = 720;
        mTestActivity
                .getResources()
                .updateConfiguration(config, mTestActivity.getResources().getDisplayMetrics());
        mTestActivity.onConfigurationChanged(config);

        // Padding should be non-zero on wide display
        int paddingStart = view.getPaddingStart();
        assertTrue("Padding should be applied on wide display", paddingStart > 0);
    }

    @Test
    @Config(qualifiers = "sw320dp") // Start with narrow display
    public void testFragmentWithNonMatchingTag_doesNotApplyPadding() {
        TestFragment fragment = new TestFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, "other_tag")
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);

        // Transition to wide
        Configuration config = new Configuration(mTestActivity.getResources().getConfiguration());
        config.screenWidthDp = 720;
        mTestActivity
                .getResources()
                .updateConfiguration(config, mTestActivity.getResources().getDisplayMetrics());
        mTestActivity.onConfigurationChanged(config);

        // Padding should remain 0
        int paddingStart = view.getPaddingStart();
        assertEquals(0, paddingStart);
    }

    @Test
    @Config(qualifiers = "sw320dp") // Start with narrow display
    public void testEmbeddablePageWithNonMatchingTag_appliesPadding() {
        // Regression test for crbug.com/563398857: settings pages that are not
        // PreferenceFragmentCompat (e.g. SearchEngineSettings) must be padded even when they are
        // not the tagged main fragment.
        TestEmbeddablePageFragment fragment = new TestEmbeddablePageFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, "other_tag")
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);

        // Transition to wide.
        Configuration config = new Configuration(mTestActivity.getResources().getConfiguration());
        config.screenWidthDp = 720;
        mTestActivity
                .getResources()
                .updateConfiguration(config, mTestActivity.getResources().getDisplayMetrics());
        mTestActivity.onConfigurationChanged(config);

        assertTrue("Padding should be applied on wide display", view.getPaddingStart() > 0);
    }

    @Test
    public void testEmbeddablePage_appliesPaddingImmediatelyOnViewCreated() {
        // EmbeddableSettingsPages (e.g. SelectLanguageFragment) must have their padding applied
        // immediately upon view creation so that the initial frame renders with the correct
        // padding and does not glitch or shift horizontally.
        TestEmbeddablePageFragment fragment = new TestEmbeddablePageFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, "other_tag")
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);
        assertTrue(
                "Padding should be applied immediately on view created without waiting for layout",
                view.getPaddingStart() > 0);
    }

    /**
     * Regression test for crbug.com/565250132: settings does not always fill the window. The
     * vertical tab strip and the side panel take horizontal space away from the tab showing
     * settings, so padding computed from the window width over-pads the content and the page
     * visibly shifts once a later layout pass recomputes it from the real container width.
     */
    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testEmbeddablePage_padsFromContainerWidthNotWindowWidth() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();
        int containerWidthPx = ViewUtils.dpToPx(metrics, 560);

        FrameLayout container = new FrameLayout(mTestActivity);
        container.setId(View.generateViewId());
        mTestActivity.setContentView(container);
        container.measure(
                View.MeasureSpec.makeMeasureSpec(containerWidthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        container.layout(0, 0, containerWidthPx, 1000);

        TestEmbeddablePageFragment fragment = new TestEmbeddablePageFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(container.getId(), fragment, "other_tag")
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);

        // The container is narrower than WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, so only the minimum
        // padding applies. Computing from the 800dp window would give (800 - 600) / 2 = 100dp.
        int minWidePaddingPx =
                mTestActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);
        assertEquals(
                "Padding should be derived from the settings container, not the window",
                minWidePaddingPx,
                view.getPaddingStart());
    }

    /**
     * Regression test for crbug.com/565658549: when a layout pass changes the padding, the children
     * must be laid out for the new padding within that same pass. A layout request made from a
     * layout change listener is dropped, which left the children positioned for the old padding.
     */
    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testEmbeddablePage_relayoutsChildrenWhenLayoutChangesPadding() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();

        // Lay out the container at the full window width first, e.g. before the vertical tab
        // strip took some of the width away.
        FrameLayout container = new FrameLayout(mTestActivity);
        container.setId(View.generateViewId());
        mTestActivity.setContentView(container);
        layoutView(container, ViewUtils.dpToPx(metrics, 800));

        TestEmbeddablePageFragment fragment = new TestEmbeddablePageFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(container.getId(), fragment, "other_tag")
                .commitNow();
        ViewGroup view = (ViewGroup) fragment.getView();
        assertNotNull(view);
        // (800 - 600) / 2 = 100dp.
        assertEquals(ViewUtils.dpToPx(metrics, 100), view.getPaddingStart());

        // Shrink the container. A single layout pass must position the child for the new padding.
        layoutView(container, ViewUtils.dpToPx(metrics, 560));
        int minWidePaddingPx =
                mTestActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);
        assertEquals(minWidePaddingPx, view.getPaddingStart());
        assertEquals(minWidePaddingPx, view.getChildAt(0).getLeft());
    }

    /**
     * Regression test for crbug.com/587191542: preference pages apply their wide display padding as
     * a RecyclerView item offset, so a layout pass that changes the offset must still position the
     * rows for the new offset within that same pass.
     */
    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testPreferencePage_relayoutsChildrenWhenLayoutChangesItemOffset() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();

        FrameLayout container = createContentContainer();
        RecyclerView recyclerView = addTestSettingsFragment(container).getListView();
        AtomicInteger layoutCount = addLayoutCounter(recyclerView);

        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(1, layoutCount.get());
        assertTrue(recyclerView.getChildCount() > 0);
        View child = recyclerView.getChildAt(0);
        // (800 - 600) / 2 = 100dp.
        assertEquals(ViewUtils.dpToPx(metrics, 100), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());

        // Steady-state layout at the same width (e.g. tapping a preference card before the rail
        // resizes) so that RecyclerView's cached item insets are no longer dirty.
        ViewUtils.requestLayout(recyclerView, "WideDisplayPaddingApplierTest");
        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(2, layoutCount.get());

        // Shrink by the 76dp collapsed vertical tab rail. (724 - 600) / 2 = 62dp.
        layoutView(container, ViewUtils.dpToPx(metrics, 724));
        assertEquals(4, layoutCount.get());
        assertEquals(ViewUtils.dpToPx(metrics, 62), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());

        ViewUtils.requestLayout(recyclerView, "WideDisplayPaddingApplierTest");
        layoutView(container, ViewUtils.dpToPx(metrics, 724));
        assertEquals(5, layoutCount.get());

        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(7, layoutCount.get());
        assertEquals(ViewUtils.dpToPx(metrics, 100), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());
    }

    /**
     * Item animations queued by the layout pass that changes the item offset must be ended before
     * the rows are laid out again, otherwise the rows animate from positions computed for the old
     * offset.
     */
    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testPreferencePage_endsRunningItemAnimationsBeforeRelayout() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();

        FrameLayout container = createContentContainer();
        TestSettingsFragment fragment = addTestSettingsFragment(container);
        RecyclerView recyclerView = fragment.getListView();
        AtomicInteger endAnimationsCount = new AtomicInteger();
        // Robolectric finishes the real view property animations synchronously, so track started
        // animations to keep them reported as running until they are explicitly ended.
        AtomicBoolean animationStarted = new AtomicBoolean();
        recyclerView.setItemAnimator(
                new DefaultItemAnimator() {
                    @Override
                    public boolean animateAdd(RecyclerView.ViewHolder holder) {
                        animationStarted.set(true);
                        return super.animateAdd(holder);
                    }

                    @Override
                    public boolean animateMove(
                            RecyclerView.ViewHolder holder,
                            int fromX,
                            int fromY,
                            int toX,
                            int toY) {
                        animationStarted.set(true);
                        return super.animateMove(holder, fromX, fromY, toX, toY);
                    }

                    @Override
                    public boolean isRunning() {
                        return animationStarted.get() || super.isRunning();
                    }

                    @Override
                    public void endAnimations() {
                        animationStarted.set(false);
                        endAnimationsCount.incrementAndGet();
                        super.endAnimations();
                    }
                });

        ViewUtils.requestLayout(recyclerView, "WideDisplayPaddingApplierTest");
        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(0, endAnimationsCount.get());

        // Insert a row above the existing one so that the next layout pass queues a pending move
        // and a pending add animation. Without a comparison callback, the adapter falls back to
        // notifyDataSetChanged(), which does not animate.
        fragment.getPreferenceManager()
                .setPreferenceComparisonCallback(
                        new PreferenceManager.SimplePreferenceComparisonCallback());
        PreferenceScreen screen = fragment.getPreferenceScreen();
        screen.getPreference(0).setOrder(1);
        Preference inserted = new Preference(fragment.getPreferenceManager().getContext());
        inserted.setTitle("Inserted preference");
        inserted.setOrder(0);
        screen.addPreference(inserted);
        ShadowLooper.runUiThreadTasks();
        assertEquals(2, recyclerView.getAdapter().getItemCount());

        layoutView(container, ViewUtils.dpToPx(metrics, 724));

        assertEquals(1, endAnimationsCount.get());
        assertFalse(recyclerView.isAnimating());
        assertEquals(2, recyclerView.getChildCount());
        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View child = recyclerView.getChildAt(i);
            assertEquals(0f, child.getTranslationX(), /* delta= */ 0f);
            assertEquals(0f, child.getTranslationY(), /* delta= */ 0f);
            // (724 - 600) / 2 = 62dp.
            assertEquals(ViewUtils.dpToPx(metrics, 62), child.getLeft());
        }
    }

    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testPreferencePage_relayoutsChildrenInTwoColumnMode() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();

        FrameLayout container = createContentContainer();
        RecyclerView recyclerView = addTestSettingsFragment(container).getListView();
        AtomicInteger layoutCount = addLayoutCounter(recyclerView);

        layoutView(container, ViewUtils.dpToPx(metrics, 1000));
        assertEquals(1, layoutCount.get());
        View child = recyclerView.getChildAt(0);
        assertNotNull(child);
        // (1000 - 600) / 2 = 200dp.
        assertEquals(ViewUtils.dpToPx(metrics, 200), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());

        // Steady-state layout at the same width so that RecyclerView's cached item insets are no
        // longer dirty.
        ViewUtils.requestLayout(recyclerView, "WideDisplayPaddingApplierTest");
        layoutView(container, ViewUtils.dpToPx(metrics, 1000));
        assertEquals(2, layoutCount.get());

        // Two-column mode uses the two-column offset at the same container width.
        // 24dp + (1000 - 840 - 48) / 2 = 80dp.
        when(mIsTwoColumnSettingsVisibleSupplier.getAsBoolean()).thenReturn(true);
        ViewUtils.requestLayout(recyclerView, "WideDisplayPaddingApplierTest");
        layoutView(container, ViewUtils.dpToPx(metrics, 1000));
        assertEquals(4, layoutCount.get());
        assertEquals(ViewUtils.dpToPx(metrics, 80), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 840), child.getWidth());

        // 24dp + (1100 - 840 - 48) / 2 = 130dp.
        layoutView(container, ViewUtils.dpToPx(metrics, 1100));
        assertEquals(6, layoutCount.get());
        assertEquals(ViewUtils.dpToPx(metrics, 130), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 840), child.getWidth());
    }

    @Test
    @Config(qualifiers = "w800dp-h1280dp")
    public void testPreferencePage_updatesItemOffsetWhenDisplayStyleChanges() {
        DisplayMetrics metrics = mTestActivity.getResources().getDisplayMetrics();

        FrameLayout container = createContentContainer();
        RecyclerView recyclerView = addTestSettingsFragment(container).getListView();
        AtomicInteger layoutCount = addLayoutCounter(recyclerView);

        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(1, layoutCount.get());
        View child = recyclerView.getChildAt(0);
        assertNotNull(child);
        // (800 - 600) / 2 = 100dp.
        assertEquals(ViewUtils.dpToPx(metrics, 100), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());

        // The narrow display style has no item offset, and it is applied by the first layout pass
        // after the change without needing a nested relayout.
        Configuration narrowConfig =
                new Configuration(mTestActivity.getResources().getConfiguration());
        narrowConfig.screenWidthDp = 320;
        mTestActivity.getResources().updateConfiguration(narrowConfig, metrics);
        mTestActivity.onConfigurationChanged(narrowConfig);
        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(2, layoutCount.get());
        assertEquals(0, child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 800), child.getWidth());

        Configuration wideConfig =
                new Configuration(mTestActivity.getResources().getConfiguration());
        wideConfig.screenWidthDp = 720;
        mTestActivity.getResources().updateConfiguration(wideConfig, metrics);
        mTestActivity.onConfigurationChanged(wideConfig);
        layoutView(container, ViewUtils.dpToPx(metrics, 800));
        assertEquals(3, layoutCount.get());
        assertEquals(ViewUtils.dpToPx(metrics, 100), child.getLeft());
        assertEquals(ViewUtils.dpToPx(metrics, 600), child.getWidth());
    }

    private static void layoutView(View view, int widthPx) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(widthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, widthPx, 1000);
    }

    @Test
    public void testShouldApplyPadding_excludesMultiColumnSettings() {
        // MultiColumnSettings hosts both columns; padding it would inset the content twice.
        assertFalse(mApplier.shouldApplyPadding(mock(MultiColumnSettings.class)));
    }

    @Test
    public void testShouldApplyPadding_includesSettingsPages() {
        assertTrue(mApplier.shouldApplyPadding(new TestSettingsFragment()));
        assertTrue(mApplier.shouldApplyPadding(new TestEmbeddablePageFragment()));
    }

    @Test
    public void testShouldApplyPadding_excludesNonSettingsFragments() {
        // Lifecycle callbacks are registered recursively, so fragments that are not settings
        // pages (most notably dialog fragments) also reach the applier.
        assertFalse(mApplier.shouldApplyPadding(new TestFragment()));
    }

    private boolean hasPaddedItemDecoration(RecyclerView recyclerView) {
        for (int i = 0; i < recyclerView.getItemDecorationCount(); i++) {
            if (recyclerView.getItemDecorationAt(i) instanceof PaddedItemDecorationWithDivider) {
                return true;
            }
        }
        return false;
    }

    private FrameLayout createContentContainer() {
        FrameLayout container = new FrameLayout(mTestActivity);
        container.setId(View.generateViewId());
        mTestActivity.setContentView(container);
        return container;
    }

    private TestSettingsFragment addTestSettingsFragment(FrameLayout container) {
        TestSettingsFragment fragment = new TestSettingsFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(container.getId(), fragment, "other_tag")
                .commitNow();
        assertNotNull(fragment.getListView());
        return fragment;
    }

    private static AtomicInteger addLayoutCounter(RecyclerView recyclerView) {
        AtomicInteger layoutCount = new AtomicInteger();
        recyclerView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                        layoutCount.incrementAndGet());
        return layoutCount;
    }
}
