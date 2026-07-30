// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link MultiColumnTitleUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
@Batch(Batch.UNIT_TESTS)
public class MultiColumnTitleUpdaterTest {

    /** Fake PreferenceFragment for testing. */
    public static class FakePreferenceFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getContext()));
        }
    }

    /** Fake Fragment for testing without inflating MainSettings. */
    @SuppressWarnings("MissingSuperCall")
    public static class FakeMultiColumnSettings extends MultiColumnSettings {
        private List<Title> mFakeTitles = new ArrayList<>();

        public FakeMultiColumnSettings() {}

        void setFakeTitles(List<Title> titles) {
            mFakeTitles = titles;
        }

        @Override
        public List<Title> getTitles() {
            return mFakeTitles;
        }

        @Override
        public boolean isTwoColumn() {
            return true;
        }

        @Override
        public View onCreateView(
                LayoutInflater inflater,
                @Nullable ViewGroup container,
                @Nullable Bundle savedInstanceState) {
            SlidingPaneLayout layout = new SlidingPaneLayout(inflater.getContext());
            FrameLayout detail = new FrameLayout(inflater.getContext());
            detail.setId(R.id.preferences_detail);
            layout.addView(detail);
            return layout;
        }

        @Override
        public void onViewCreated(View view, Bundle savedInstanceState) {}

        @Override
        public PreferenceFragmentCompat onCreateInitialDetailFragment() {
            return new FakePreferenceFragment();
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock public Callback<String> mTitleTapCallback;

    private TestActivity mActivity;
    private FakeMultiColumnSettings mMultiColumnSettings;
    private LinearLayout mContainer;

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> mActivity = (TestActivity) activity);
        mContainer = new LinearLayout(mActivity);

        mMultiColumnSettings = new FakeMultiColumnSettings();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mMultiColumnSettings, "settings")
                .commitNow();

        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .add(R.id.preferences_detail, new Fragment())
                .addToBackStack("appearance_entry")
                .commit();
        mMultiColumnSettings.getChildFragmentManager().executePendingTransactions();
    }

    private static SettableMonotonicObservableSupplier<String> createTitleSupplier(String title) {
        SettableMonotonicObservableSupplier<String> supplier =
                ObservableSuppliers.createMonotonic();
        supplier.set(title);
        return supplier;
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testSingleTitle_noBackButton() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater =
                new MultiColumnTitleUpdater(
                        /* savedInstanceState= */ null,
                        mMultiColumnSettings,
                        mActivity,
                        mContainer,
                        /* mainTitleSetter= */ (t) -> {},
                        /* titleTapCallback= */ mTitleTapCallback,
                        /* initialBreadcrumbPath= */ null);

        updater.onTitleUpdated();

        // Single title should only have 1 child (the DetailedTitle view, no back button).
        assertEquals(1, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof TextView);
        assertFalse(mContainer.getChildAt(0).isClickable());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMultipleTitles_showsBackButtonAndNavigatesToPreviousDetailPane() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(new MultiColumnSettings.Title("uuid2", createTitleSupplier("Theme"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater =
                new MultiColumnTitleUpdater(
                        /* savedInstanceState= */ null,
                        mMultiColumnSettings,
                        mActivity,
                        mContainer,
                        /* mainTitleSetter= */ (t) -> {},
                        /* titleTapCallback= */ mTitleTapCallback,
                        /* initialBreadcrumbPath= */ null);

        updater.onTitleUpdated();

        // Multiple titles should prepend the back button as child at index 0.
        assertTrue(mContainer.getChildCount() > 1);
        assertTrue(mContainer.getChildAt(0) instanceof ChromeImageButton);

        // Back button should be offset to the left.
        ChromeImageButton backButton = (ChromeImageButton) mContainer.getChildAt(0);
        var layoutParams = (LinearLayout.LayoutParams) backButton.getLayoutParams();
        assertTrue(layoutParams.getMarginStart() < 0);

        // Parent title ("Appearance") should be clickable, but active title ("Theme") should not.
        assertTrue(mContainer.getChildAt(1) instanceof TextView);
        assertEquals("Appearance", ((TextView) mContainer.getChildAt(1)).getText().toString());
        assertTrue(mContainer.getChildAt(1).isClickable());

        assertTrue(mContainer.getChildAt(3) instanceof TextView);
        assertEquals("Theme", ((TextView) mContainer.getChildAt(3)).getText().toString());
        assertFalse(mContainer.getChildAt(3).isClickable());

        // Clicking back button should pop to previous title ("Appearance") and trigger callback.
        backButton.performClick();
        verify(mTitleTapCallback).onResult("appearance_entry");
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMultipleTitles_featureDisabled_noBackButton() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(new MultiColumnSettings.Title("uuid2", createTitleSupplier("Theme"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater =
                new MultiColumnTitleUpdater(
                        /* savedInstanceState= */ null,
                        mMultiColumnSettings,
                        mActivity,
                        mContainer,
                        /* mainTitleSetter= */ (t) -> {},
                        /* titleTapCallback= */ mTitleTapCallback,
                        /* initialBreadcrumbPath= */ null);

        updater.onTitleUpdated();

        // When SettingsInTab is disabled, back button is not shown (first child is TextView).
        assertTrue(mContainer.getChildAt(0) instanceof TextView);
    }
}
