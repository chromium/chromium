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
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
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

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.ui.base.ViewUtils;

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
            return new View(requireContext());
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
}
