// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import com.google.android.material.appbar.AppBarLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MultiColumnSettings;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;

/** Unit tests for {@link SettingsSearchCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsSearchCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FragmentActivity mActivity;
    private Toolbar mToolbar;
    @Mock private MultiColumnSettings mMultiColumnSettings;
    @Mock private Profile mProfile;
    @Mock private ModalDialogManager mModalDialogManager;

    private SettingsSearchCoordinator mCoordinator;
    private boolean mUseMultiColumn = true;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FragmentActivity.class).create().get();
        mActivity.setTheme(R.style.Theme_Chromium_Settings);

        FrameLayout rootView = new FrameLayout(mActivity);
        rootView.setId(R.id.settings_activity);

        AppBarLayout appBarLayout = new AppBarLayout(mActivity);
        appBarLayout.setId(R.id.app_bar_layout);

        // Create a real Toolbar and a title inside it, so that ToolbarUtils.getTitleTextView() can
        // find it and not return null (which would crash in constructor's setFragmentState call).
        mToolbar = new Toolbar(mActivity);
        mToolbar.setId(R.id.action_bar);
        mToolbar.setTitle("Settings");
        TextView titleView = new TextView(mActivity);
        titleView.setText("Settings");
        mToolbar.addView(titleView);
        mToolbar.addView(new ActionMenuView(mActivity));

        appBarLayout.addView(mToolbar);
        rootView.addView(appBarLayout);

        FrameLayout headerPane = new FrameLayout(mActivity);
        headerPane.setId(R.id.preferences_header);
        rootView.addView(headerPane);

        FrameLayout detailPane = new FrameLayout(mActivity);
        detailPane.setId(R.id.preferences_detail);
        rootView.addView(detailPane);

        mActivity.setContentView(rootView);

        SettableMonotonicObservableSupplier<ModalDialogManager> modalDialogSupplier =
                ObservableSuppliers.createMonotonic();
        modalDialogSupplier.set(mModalDialogManager);

        mCoordinator =
                new SettingsSearchCoordinator(
                        mActivity,
                        mToolbar,
                        () -> mUseMultiColumn,
                        mMultiColumnSettings,
                        new HashMap<>(),
                        mProfile,
                        (index) -> {},
                        modalDialogSupplier);
    }

    @After
    public void tearDown() {
        SettingsIndexData.reset();
        // Avoid runnable pollution between tests.
        ShadowLooper.idleMainLooper();
    }

    /**
     * Sets up the mock {@link MultiColumnSettings} with a valid child {@link FragmentManager} and
     * {@link SlidingPaneLayout}, and measures/lays out the root view so search UI initialization
     * and width calculations can execute properly.
     */
    private void setUpMultiColumnSettings() {
        FragmentManager childFragmentManager = mActivity.getSupportFragmentManager();
        when(mMultiColumnSettings.getChildFragmentManagerOrNull()).thenReturn(childFragmentManager);

        SlidingPaneLayout slidingPaneLayout = new SlidingPaneLayout(mActivity);
        when(mMultiColumnSettings.getView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.requireView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.getSlidingPaneLayout()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);

        View rootView = mActivity.findViewById(R.id.settings_activity);
        int widthSpec = View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY);
        rootView.measure(widthSpec, heightSpec);
        rootView.layout(0, 0, 1000, 600);
    }

    @Test
    public void
            testDisableBackgroundTalkbackNavigation_whenMultiColumnSettingsNotAdded_doesNotCrash() {
        // Mock multiColumnSettings to return null (not attached).
        when(mMultiColumnSettings.getChildFragmentManagerOrNull()).thenReturn(null);

        // This call should not crash even if the fragment manager is null.
        mCoordinator.disableBackgroundTalkbackNavigation();
    }

    @Test
    public void testAccessibilityStateChanged_whenMultiColumnSettingsAdded_doesNotCrash() {
        // Mock multiColumnSettings to be attached and return a child fragment manager.
        FragmentManager childFragmentManager = mock(FragmentManager.class);
        when(mMultiColumnSettings.getChildFragmentManagerOrNull()).thenReturn(childFragmentManager);
        when(childFragmentManager.isStateSaved()).thenReturn(false);

        var state =
                new AccessibilityState.State(
                        /* isComplexUserInteractionServiceEnabled= */ false,
                        /* isTouchExplorationEnabled= */ false,
                        /* isPerformGesturesEnabled= */ false,
                        /* isAnyAccessibilityServiceEnabled= */ false,
                        /* isAccessibilityToolPresent= */ false,
                        /* isTextShowPasswordEnabled= */ false,
                        /* isOnlyAutofillRunning= */ false,
                        /* isOnlyPasswordManagersEnabled= */ false,
                        /* isKnownScreenReaderEnabled= */ false,
                        /* isSamsungTalkBackEnabled= */ false);

        // This call should not crash.
        mCoordinator.onAccessibilityStateChanged(state, state);
    }

    @Test
    public void testOnHeaderLayoutUpdated_whenSearchBoxNotInitialized_doesNotCrash() {
        // Calling onHeaderLayoutUpdated before search box is inflated should return cleanly without
        // crashing.
        mCoordinator.onHeaderLayoutUpdated();
    }

    @Test
    public void testOnHeaderLayoutUpdated_switchesToSingleColumnMode() {
        FragmentManager childFragmentManager = mock(FragmentManager.class);
        when(mMultiColumnSettings.getChildFragmentManagerOrNull()).thenReturn(childFragmentManager);

        SlidingPaneLayout slidingPaneLayout = new SlidingPaneLayout(mActivity);
        when(mMultiColumnSettings.getView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.requireView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.getSlidingPaneLayout()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);

        // Start in multi-column mode.
        mUseMultiColumn = true;
        mCoordinator.initializeSearchUi(null);

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);
        assertEquals(mToolbar, searchBox.getParent());

        // Switch to single-column mode and notify via onHeaderLayoutUpdated().
        mUseMultiColumn = false;
        mCoordinator.onHeaderLayoutUpdated();
        Robolectric.flushForegroundThreadScheduler();

        View appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
        assertEquals(appBarLayout, searchBox.getParent());
    }

    @Test
    public void testEmptyFragmentClear_whenViewsDetached_doesNotCrash() {
        EmptyFragment emptyFragment = new EmptyFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(emptyFragment, "empty")
                .commitNow();
        // Clear when R.id.empty_state_icon is not present in Activity view hierarchy.
        emptyFragment.clear();
    }

    @Test
    public void testSingleColumnSearchUiWidth_updatesOnAppBarLayoutResized() {
        FragmentManager childFragmentManager = mock(FragmentManager.class);
        when(mMultiColumnSettings.getChildFragmentManagerOrNull()).thenReturn(childFragmentManager);

        SlidingPaneLayout slidingPaneLayout = new SlidingPaneLayout(mActivity);
        when(mMultiColumnSettings.getView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.requireView()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.getSlidingPaneLayout()).thenReturn(slidingPaneLayout);
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);

        // Start in single-column mode.
        mUseMultiColumn = false;
        mCoordinator.initializeSearchUi(null);

        View appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
        assertNotNull(appBarLayout);
        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);

        int minPadding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        View rootView = mActivity.findViewById(R.id.settings_activity);
        assertNotNull(rootView);

        // Start with medium width.
        int widthSpec = View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY);
        rootView.measure(widthSpec, heightSpec);
        rootView.layout(0, 0, 400, 100);
        ShadowLooper.idleMainLooper();

        // Verify search box has correct initial layout.
        var lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
        assertEquals(minPadding, lp.getMarginStart());
        assertEquals(minPadding, lp.getMarginEnd());
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, lp.width);

        // Simulate available width shrinking (e.g. side panel opening) to 300px.
        widthSpec = View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY);
        rootView.measure(widthSpec, heightSpec);
        rootView.layout(0, 0, 300, 100);
        ShadowLooper.idleMainLooper();

        // Search box width should adjust to the narrower container width while maintaining
        // standard min padding on narrow screens.
        lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
        assertEquals(minPadding, lp.getMarginStart());
        assertEquals(minPadding, lp.getMarginEnd());
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, lp.width);

        // Simulate expanding to a wide display (e.g. 1000px).
        widthSpec = View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY);
        rootView.measure(widthSpec, heightSpec);
        rootView.layout(0, 0, 1000, 100);
        ShadowLooper.idleMainLooper();

        // Search box width should adjust to the wider container width with appropriate
        // margins.
        int itemMargin =
                mActivity.getResources().getDimensionPixelSize(R.dimen.settings_item_margin);
        int expectedMargin = (1000 - 600) / 2 + itemMargin;
        lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
        assertEquals(expectedMargin, lp.getMarginStart());
        assertEquals(expectedMargin, lp.getMarginEnd());
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, lp.width);
    }

    /** Regression test for https://crbug.com/545872336. */
    @Test
    public void testUpdateHelpMenuVisibility_whenDestroyed_doesNotCrash() {
        mUseMultiColumn = false;
        mCoordinator.updateHelpMenuVisibility();

        // Destroy the coordinator before the posted Runnable executes on the Looper (e.g. during
        // theme change / Activity recreation).
        mCoordinator.destroy();

        // Flush the looper. The posted task should exit early without calling isLayoutOpen(),
        // which could cause a crash due to MultiColumnSettings not yet having a view.
        ShadowLooper.idleMainLooper();

        verify(mMultiColumnSettings, never()).isLayoutOpen();
    }

    /** Regression test for https://crbug.com/545907093. */
    @Test
    public void testOnConfigurationChangedInternal_whenDestroyed_doesNotCrash() {
        // Switch to single-column mode and trigger configuration change handling.
        mUseMultiColumn = false;
        mCoordinator.onConfigurationChangedInternal();

        // Destroy the coordinator before the posted Runnable executes on the Looper (for example,
        // language switch / Activity recreation).
        mCoordinator.destroy();

        // Flush the looper. The posted task should exit early without crashing on methods that
        // require views that are no longer present.
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testShouldShowNavigationIcon_multiColumn() {
        mCoordinator.setUseMultiColumnForTesting(true);

        // In multi-column mode, navigation icon should always be shown regardless of state.
        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_SETTINGS);
        assertTrue(mCoordinator.shouldShowNavigationIcon());

        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_SEARCH);
        assertTrue(mCoordinator.shouldShowNavigationIcon());

        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_RESULTS);
        assertTrue(mCoordinator.shouldShowNavigationIcon());
    }

    @Test
    public void testShouldShowNavigationIcon_singleColumn() {
        mCoordinator.setUseMultiColumnForTesting(false);

        // In default state (FS_SETTINGS), navigation icon should be shown.
        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_SETTINGS);
        assertTrue(mCoordinator.shouldShowNavigationIcon());

        // In search state (FS_SEARCH), navigation icon should be hidden.
        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_SEARCH);
        assertFalse(mCoordinator.shouldShowNavigationIcon());

        // In results state (FS_RESULTS), navigation icon should be shown (as a back button).
        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_RESULTS);
        assertTrue(mCoordinator.shouldShowNavigationIcon());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testInitializeSearchUi_withSettingsInTab_setsSearchBoxFocusable() {
        setUpMultiColumnSettings();
        mCoordinator.initializeSearchUi(null);

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);
        assertTrue(searchBox.isFocusable());
        assertTrue(searchBox.isFocusableInTouchMode());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testInitializeSearchUi_withoutSettingsInTab_doesNotSetSearchBoxFocusable() {
        setUpMultiColumnSettings();
        mCoordinator.initializeSearchUi(null);

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);
        assertFalse(searchBox.isFocusableInTouchMode());
    }

    @Test
    public void testOnSlideStateUpdated_singleColumn_updatesSearchBoxVisibility() {
        setUpMultiColumnSettings();
        mUseMultiColumn = false;
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);
        mCoordinator.initializeSearchUi(null);
        ShadowLooper.idleMainLooper();

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);
        assertEquals(View.VISIBLE, searchBox.getVisibility());

        // Detail pane opens.
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(true);
        mCoordinator.onSlideStateUpdated(MultiColumnSettings.SlideState.OPENED);
        assertEquals(View.GONE, searchBox.getVisibility());

        // Detail pane closes.
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);
        mCoordinator.onSlideStateUpdated(MultiColumnSettings.SlideState.CLOSED);
        assertEquals(View.VISIBLE, searchBox.getVisibility());
    }

    @Test
    public void testOnHeaderLayoutUpdated_singleColumn_updatesSearchBoxVisibility() {
        setUpMultiColumnSettings();
        mUseMultiColumn = false;
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(true);
        mCoordinator.initializeSearchUi(null);
        ShadowLooper.idleMainLooper();

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);
        assertEquals(View.GONE, searchBox.getVisibility());

        // Header layout updated when showing main settings.
        when(mMultiColumnSettings.isLayoutOpen()).thenReturn(false);
        mCoordinator.onHeaderLayoutUpdated();
        assertEquals(View.VISIBLE, searchBox.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testExitSearchState_withSettingsInTab_multiColumn_focusesSearchBox() {
        setUpMultiColumnSettings();
        mUseMultiColumn = true;
        mCoordinator.initializeSearchUi(null);
        ShadowLooper.idleMainLooper();

        View searchBox = mActivity.findViewById(R.id.search_box);
        assertNotNull(searchBox);

        mCoordinator.setFragmentState(SettingsSearchCoordinator.FS_SEARCH);
        searchBox.setVisibility(View.GONE);

        mCoordinator.exitSearchState();
        assertEquals(View.VISIBLE, searchBox.getVisibility());
        assertTrue(searchBox.isFocused());
    }

    /**
     * Subclass of Fragment to represent the initial detail pane fragment (e.g. Google services).
     */
    public static class TestDetailFragment extends Fragment {
        public TestDetailFragment() {}
    }

    @Test
    public void testSearchInMultiColumnThenExitSearchRestoresDetailFragment() {
        // Initialize an empty SettingsIndexData and mark it as indexed to prevent
        // enterSearchState() from attempting to build the real search index across all
        // registered settings fragments in SearchIndexProviderRegistry (which requires
        // native/feature flag configuration in unit tests).
        SettingsIndexData.createInstance().resetNeedsIndexing();

        setUpMultiColumnSettings();
        mUseMultiColumn = true;

        // Add initial detail fragment representing Google services in the detail pane.
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        TestDetailFragment initialDetailFragment = new TestDetailFragment();
        fragmentManager
                .beginTransaction()
                .add(R.id.preferences_detail, initialDetailFragment)
                .commitNow();

        mCoordinator.initializeSearchUi(null);
        ShadowLooper.idleMainLooper();

        // Verify initial UI state in multi-column mode.
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        assertNotNull(searchBox);
        assertNotNull(queryContainer);
        assertEquals(View.VISIBLE, searchBox.getVisibility());
        assertEquals(View.GONE, queryContainer.getVisibility());
        assertNotNull(SettingsIndexData.getInstance());
        assertFalse(SettingsIndexData.getInstance().needsIndexing());

        // 1. Click search box to enter search state in multi-column mode.
        searchBox.performClick();
        fragmentManager.executePendingTransactions();
        ShadowLooper.idleMainLooper();

        assertEquals(View.GONE, searchBox.getVisibility());
        assertEquals(View.VISIBLE, queryContainer.getVisibility());

        // 2. Enter search query and simulate search results appearing.
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        assertNotNull(queryEdit);
        queryEdit.setText("Theme");

        var entry =
                new SettingsIndexData.Entry.Builder(
                                "theme_id", "theme_key", "Theme", "MainSettings")
                        .build();
        var results = new SettingsIndexData.SearchResults();
        results.addItem(entry, 100);
        mCoordinator.displayResultsFragment(results);
        fragmentManager.executePendingTransactions();
        ShadowLooper.idleMainLooper();

        // Verify search results fragment is displayed in the detail container.
        Fragment resultFragment =
                fragmentManager.findFragmentByTag(SettingsSearchCoordinator.RESULT_FRAGMENT);
        assertNotNull(resultFragment);
        assertTrue(resultFragment instanceof SearchResultsPreferenceFragment);
        assertEquals(resultFragment, fragmentManager.findFragmentById(R.id.preferences_detail));

        // 3. Click back arrow icon to exit search.
        View backArrow = mActivity.findViewById(R.id.back_arrow_icon);
        assertNotNull(backArrow);
        backArrow.performClick();
        fragmentManager.executePendingTransactions();
        ShadowLooper.idleMainLooper();

        // 4. Verify search box is restored and search query container is hidden.
        assertEquals(View.VISIBLE, searchBox.getVisibility());
        assertEquals(View.GONE, queryContainer.getVisibility());

        // 5. Verify search results fragment is removed.
        assertNull(fragmentManager.findFragmentByTag(SettingsSearchCoordinator.RESULT_FRAGMENT));

        // 6. Verify initial detail fragment is restored in the detail pane.
        Fragment currentDetail = fragmentManager.findFragmentById(R.id.preferences_detail);
        assertNotNull(currentDetail);
        assertEquals(initialDetailFragment, currentDetail);
    }
}
