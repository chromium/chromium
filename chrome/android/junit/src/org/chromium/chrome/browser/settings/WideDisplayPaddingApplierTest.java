// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;

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
    public static class TestPreferenceFragment extends PreferenceFragmentCompat {
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
    public void testPreferenceFragment_appliesPaddingOnGlobalLayout() {
        TestPreferenceFragment fragment = new TestPreferenceFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);
        RecyclerView recyclerView = view.findViewById(R.id.recycler_view);
        assertNotNull(recyclerView);

        // Initially no padded decoration.
        assertFalse(hasPaddedItemDecoration(recyclerView));

        // Trigger global layout to execute WideDisplayPadding.apply().
        view.getViewTreeObserver().dispatchOnGlobalLayout();

        // Now it should have the padded decoration.
        assertTrue(hasPaddedItemDecoration(recyclerView));
    }

    @Test
    @Config(qualifiers = "sw320dp") // Start with narrow display
    public void testMainFragmentWithMatchingTag_appliesPaddingOnGlobalLayout() {
        TestFragment fragment = new TestFragment();
        mTestActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, MAIN_TAG)
                .commitNow();

        View view = fragment.getView();
        assertNotNull(view);

        // Trigger global layout to execute WideDisplayPadding.apply().
        view.getViewTreeObserver().dispatchOnGlobalLayout();

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

        // Trigger global layout to execute WideDisplayPadding.apply().
        view.getViewTreeObserver().dispatchOnGlobalLayout();

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

    private boolean hasPaddedItemDecoration(RecyclerView recyclerView) {
        for (int i = 0; i < recyclerView.getItemDecorationCount(); i++) {
            if (recyclerView.getItemDecorationAt(i) instanceof PaddedItemDecorationWithDivider) {
                return true;
            }
        }
        return false;
    }
}
