// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.appcompat.widget.SearchView;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.settings.SelectLanguageFragment;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link MultiColumnTitleUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
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
        private View mDetailView;

        void setDetailView(View detailView) {
            mDetailView = detailView;
        }

        @Override
        public View getDetailView() {
            return mDetailView != null ? mDetailView : super.getDetailView();
        }

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

    /**
     * Creates a MultiColumnTitleUpdater with null savedInstanceState and no breadcrumb path. Exists
     * to keep tests concise.
     */
    private MultiColumnTitleUpdater createMultiColumnTitleUpdater() {
        return createMultiColumnTitleUpdater(
                /* savedInstanceState= */ null,
                /* initialBreadcrumbPath= */ null,
                /* onSearchVisibilityChanged= */ null);
    }

    /** Creates a MultiColumnTitleUpdater. Exists to keep tests concise. */
    private MultiColumnTitleUpdater createMultiColumnTitleUpdater(
            @Nullable Bundle savedInstanceState,
            @Nullable List<SettingsIndexData.Entry> initialBreadcrumbPath,
            @Nullable Runnable onSearchVisibilityChanged) {
        return new MultiColumnTitleUpdater(
                savedInstanceState,
                mMultiColumnSettings,
                mContainer,
                /* mainTitleSetter= */ (t) -> {},
                /* titleTapCallback= */ mTitleTapCallback,
                initialBreadcrumbPath,
                onSearchVisibilityChanged);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testSingleTitle_noBackButton() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.onTitleUpdated();

        // Single title should only have 1 child (the DetailedTitle view, no back button).
        assertEquals(1, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof TextView);
        assertFalse(mContainer.getChildAt(0).isClickable());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMultipleTitles_settingsInTabEnabled_showsBackButtonAndLastTitle() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(new MultiColumnSettings.Title("uuid2", createTitleSupplier("Theme"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.onTitleUpdated();

        // When SettingsInTab is enabled, only the back button and the last title should be shown.
        assertEquals(2, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof ChromeImageButton);

        // Back button's left edge should align with the parent so its ripple is not clipped. See
        // https://crbug.com/542040289 which was caused by assigning a negative margin.
        ChromeImageButton backButton = (ChromeImageButton) mContainer.getChildAt(0);
        var layoutParams = (LinearLayout.LayoutParams) backButton.getLayoutParams();
        assertEquals(0, layoutParams.getMarginStart());

        // Last title should be shown.
        assertTrue(mContainer.getChildAt(1) instanceof TextView);
        assertEquals("Theme", ((TextView) mContainer.getChildAt(1)).getText().toString());
        assertFalse(mContainer.getChildAt(1).isClickable());

        // Clicking back button should pop to previous title ("Appearance") and trigger callback.
        backButton.performClick();
        verify(mTitleTapCallback).onResult("appearance_entry");
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP})
    public void testMultipleTitles_settingsInTabDisabled_noBackButton() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(new MultiColumnSettings.Title("uuid2", createTitleSupplier("Theme"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.onTitleUpdated();

        // When SettingsInTab is disabled, back button is not shown and all titles are added with
        // separators.
        assertEquals(3, mContainer.getChildCount());

        assertTrue(mContainer.getChildAt(0) instanceof TextView);
        assertEquals("Appearance", ((TextView) mContainer.getChildAt(0)).getText().toString());
        assertTrue(mContainer.getChildAt(0).isClickable());

        assertTrue(mContainer.getChildAt(1) instanceof ImageView);

        assertTrue(mContainer.getChildAt(2) instanceof TextView);
        assertEquals("Theme", ((TextView) mContainer.getChildAt(2)).getText().toString());
        assertFalse(mContainer.getChildAt(2).isClickable());

        TextView parentTitle = (TextView) mContainer.getChildAt(0);
        parentTitle.performClick();

        // Clicking parent title ("Appearance") should pop to previous title and trigger callback.
        verify(mTitleTapCallback).onResult("appearance_entry");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testSearchResults_settingsInTabEnabled_noBackButton() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(
                new MultiColumnSettings.Title(
                        "uuid2", createTitleSupplier("Search results"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.setFirstVisibleTitleIndex(1);
        updater.onTitleUpdated();

        // When viewing Search results (mFirstVisibleTitleIndex = 1), back button should be hidden.
        // Container should only contain 1 DetailedTitle ("Search results").
        assertEquals(1, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof TextView);
        assertEquals("Search results", ((TextView) mContainer.getChildAt(0)).getText().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void
            testDetailPageFromSearchResults_settingsInTabEnabled_showsBackButtonToSearchResults() {
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        titles.add(
                new MultiColumnSettings.Title(
                        "uuid2", createTitleSupplier("Search results"), 1, null));
        titles.add(new MultiColumnSettings.Title("uuid3", createTitleSupplier("Theme"), 2, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.setFirstVisibleTitleIndex(1);
        updater.onTitleUpdated();

        // When viewing detail page from search results, back button should be shown pointing to
        // Search results.
        assertEquals(2, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof ChromeImageButton);
        assertTrue(mContainer.getChildAt(1) instanceof TextView);
        assertEquals("Theme", ((TextView) mContainer.getChildAt(1)).getText().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testSelectSettingsElementAfterSearch_showsTitle() {
        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        // Focusing or tapping the search box triggers SettingsSearchCoordinator.enterSearchState(),
        // which calls mUpdateFirstVisibleTitle.onResult(stackCount + 1). In two-column mode with
        // the initial root detail fragment, stackCount is 0, so the first visible title index is
        // set to 1 so that breadcrumb titles preceding search results are hidden.
        updater.setFirstVisibleTitleIndex(1);

        // User clicks on a category from MainSettings (e.g. "Privacy and security").
        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title(
                        "uuid1", createTitleSupplier("Privacy and security"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);
        updater.onTitleUpdated();

        // The detailed pane title should appear and not be hidden.
        assertEquals(1, mContainer.getChildCount());
        assertTrue(mContainer.getChildAt(0) instanceof TextView);
        assertEquals(
                "Privacy and security", ((TextView) mContainer.getChildAt(0)).getText().toString());
    }

    public static class TestSearchViewProviderFragment extends Fragment
            implements SearchViewProvider {
        private @Nullable SearchView mSearchView;
        private SearchViewProvider.@Nullable Observer mObserver;

        @Override
        public View onCreateView(
                LayoutInflater inflater,
                @Nullable ViewGroup container,
                @Nullable Bundle savedInstanceState) {
            return new View(inflater.getContext());
        }

        @Override
        public void setSearchViewObserver(SearchViewProvider.Observer observer) {
            mObserver = observer;
        }

        @Override
        public void initSearchView(SearchView searchView) {
            mSearchView = searchView;
        }

        public @Nullable SearchView getSearchView() {
            return mSearchView;
        }

        public SearchViewProvider.@Nullable Observer getObserver() {
            return mObserver;
        }
    }

    public static class TestSelectLanguageFragment extends SelectLanguageFragment {
        private @Nullable SearchView mSearchView;

        @Override
        public View onCreateView(
                LayoutInflater inflater,
                @Nullable ViewGroup container,
                @Nullable Bundle savedInstanceState) {
            return new View(inflater.getContext());
        }

        @Override
        public void initSearchView(SearchView searchView) {
            mSearchView = searchView;
        }

        public @Nullable SearchView getSearchView() {
            return mSearchView;
        }
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.SETTINGS_IN_TAB,
        ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS
    })
    public void testSelectLanguageFragment_addsSearchButtonAndSearchView() {
        TestSelectLanguageFragment selectLanguageFragment = new TestSelectLanguageFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, selectLanguageFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title(
                        "uuid1", createTitleSupplier("Select language"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.onTitleUpdated();

        // 1 DetailedTitle ("Select language") + 1 search button + 1 search view = 3 views in
        // mContainer.
        assertEquals(3, mContainer.getChildCount());
        assertNotNull(selectLanguageFragment.getSearchView());
        assertNull(selectLanguageFragment.getSearchView().getBackground());
        var titleParams = (LinearLayout.LayoutParams) mContainer.getChildAt(0).getLayoutParams();
        assertEquals(1f, titleParams.weight, 0.01f);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_addsSearchButtonAndSearchView() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();

        updater.onTitleUpdated();

        // 1 DetailedTitle ("All Sites") + 1 search button + 1 search view = 3 views in
        // mContainer.
        assertEquals(3, mContainer.getChildCount());
        SearchView searchView = searchViewProviderFragment.getSearchView();
        assertNotNull(searchView);
        assertNull(searchView.getBackground());
        View searchPlate = searchView.findViewById(R.id.search_plate);
        assertNotNull(searchPlate);
        assertNull(searchPlate.getBackground());
        assertEquals(mActivity.getString(R.string.search), searchView.getQueryHint());
        var titleParams = (LinearLayout.LayoutParams) mContainer.getChildAt(0).getLayoutParams();
        assertEquals(1f, titleParams.weight, 0.01f);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_openAndCloseWithBackButton() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title(
                        "uuid1", createTitleSupplier("Site Settings"), 0, null));
        titles.add(
                new MultiColumnSettings.Title("uuid2", createTitleSupplier("JavaScript"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        // 1 back button + 1 DetailedTitle ("JavaScript") + 1 search button + 1 search view = 4
        // views.
        assertEquals(4, mContainer.getChildCount());
        ChromeImageButton backButton = (ChromeImageButton) mContainer.getChildAt(0);
        View titleView = mContainer.getChildAt(1);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(2);
        SearchView searchView = (SearchView) mContainer.getChildAt(3);

        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
        assertEquals(View.GONE, searchView.getVisibility());

        // Clicking search button opens search.
        searchButton.performClick();
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(View.GONE, searchButton.getVisibility());
        assertEquals(View.VISIBLE, searchView.getVisibility());

        // Clicking back button when search is open closes search view, instead of navigating back.
        // This is equivalent to what we do on mobile.
        backButton.performClick();
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
        assertEquals(View.GONE, searchView.getVisibility());
        verify(mTitleTapCallback, never()).onResult(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_openAndCloseWithOnBackPressed() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        View titleView = mContainer.getChildAt(0);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);

        // Clicking search button opens search.
        searchButton.performClick();
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(View.GONE, searchButton.getVisibility());
        assertEquals(View.VISIBLE, searchView.getVisibility());

        // Pressing back dispatcher closes search.
        mActivity.getOnBackPressedDispatcher().onBackPressed();
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_openAndCloseWithEscapeKey() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();
        mActivity.setContentView(mContainer);

        View titleView = mContainer.getChildAt(0);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        View searchSrcTextView = searchView.findViewById(R.id.search_src_text);
        assertNotNull(searchSrcTextView);

        // Clicking search button opens search.
        assertFalse(updater.isSearchOpen());
        searchButton.performClick();
        assertTrue(updater.isSearchOpen());
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(View.GONE, searchButton.getVisibility());
        assertEquals(View.VISIBLE, searchView.getVisibility());

        // Pressing ESC on the search input field closes search and releases focus.
        KeyEvent downEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
        assertTrue(searchSrcTextView.dispatchKeyEvent(downEvent));
        assertFalse(updater.isSearchOpen());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
        assertFalse(searchButton.isFocused());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_handleBackAction() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        AtomicInteger visibilityChangeCount = new AtomicInteger(0);
        MultiColumnTitleUpdater updater =
                createMultiColumnTitleUpdater(
                        /* savedInstanceState= */ null,
                        /* initialBreadcrumbPath= */ null,
                        visibilityChangeCount::incrementAndGet);
        updater.onTitleUpdated();
        mActivity.setContentView(mContainer);

        // When search is not open, handleBackAction() returns false.
        assertFalse(updater.isSearchOpen());
        assertFalse(updater.handleBackAction());

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        searchButton.performClick();
        assertTrue(updater.isSearchOpen());
        int countAfterOpen = visibilityChangeCount.get();
        assertTrue(countAfterOpen > 0);

        // When search is open, handleBackAction() closes search and returns true.
        assertTrue(updater.handleBackAction());
        assertFalse(updater.isSearchOpen());
        assertEquals(countAfterOpen + 1, visibilityChangeCount.get());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testSearchViewProvider_openAndCloseWithObserver() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        View titleView = mContainer.getChildAt(0);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);

        assertNotNull(searchViewProviderFragment.getObserver());

        // Clicking search button opens search.
        searchButton.performClick();
        assertEquals(View.VISIBLE, searchView.getVisibility());

        // Notifying observer that search closed hides search view.
        searchViewProviderFragment.getObserver().onUpdated(false);
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testCloseSearch() {
        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("All Sites"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        View titleView = mContainer.getChildAt(0);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);

        // Clicking search button opens search.
        searchButton.performClick();
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(View.GONE, searchButton.getVisibility());
        assertEquals(View.VISIBLE, searchView.getVisibility());
        assertTrue(updater.isSearchOpen());

        // Calling closeSearch closes search and restores the title and search button.
        updater.closeSearch();
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
        assertFalse(updater.isSearchOpen());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMaybeUpdateMargins_accountsForBackButtonOffset() {
        FrameLayout detailView = new FrameLayout(mActivity);
        detailView.layout(0, 0, 1000, 100);
        View recyclerView = new View(mActivity);
        recyclerView.setId(R.id.recycler_view);
        recyclerView.layout(0, 0, 1000, 100);
        detailView.addView(recyclerView);
        mMultiColumnSettings.setDetailView(detailView);

        RelativeLayout rootLayout = new RelativeLayout(mActivity);
        HorizontalScrollView titleScrollView = new HorizontalScrollView(mActivity);
        RelativeLayout.LayoutParams scrollParams =
                new RelativeLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rootLayout.addView(titleScrollView, scrollParams);
        titleScrollView.addView(mContainer);

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        var paramsWithoutBack = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        int marginWithoutBack = paramsWithoutBack.getMarginStart();

        titles.add(new MultiColumnSettings.Title("uuid2", createTitleSupplier("Theme"), 1, null));
        mMultiColumnSettings.setFakeTitles(titles);
        updater.onTitleUpdated();

        var paramsWithBack = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        int marginWithBack = paramsWithBack.getMarginStart();

        int minTouchTargetPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        ChromeImageButton backButton = (ChromeImageButton) mContainer.getChildAt(0);
        int iconWidthPx = backButton.getDrawable().getIntrinsicWidth();
        int expectedOffset = (minTouchTargetPx - iconWidthPx) / 2;

        // Verify that the title scroll view start margin is shifted left by expectedOffset when the
        // back button is shown.
        assertEquals(marginWithoutBack - expectedOffset, marginWithBack);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMaybeUpdateMargins_accountsForSearchButtonOffset() {
        FrameLayout detailView = new FrameLayout(mActivity);
        detailView.layout(0, 0, 1000, 100);
        View recyclerView = new View(mActivity);
        recyclerView.setId(R.id.recycler_view);
        recyclerView.layout(0, 0, 1000, 100);
        detailView.addView(recyclerView);
        mMultiColumnSettings.setDetailView(detailView);

        RelativeLayout rootLayout = new RelativeLayout(mActivity);
        HorizontalScrollView titleScrollView = new HorizontalScrollView(mActivity);
        RelativeLayout.LayoutParams scrollParams =
                new RelativeLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rootLayout.addView(titleScrollView, scrollParams);
        titleScrollView.addView(mContainer);

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        var paramsWithoutSearch = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        int marginEndWithoutSearch = paramsWithoutSearch.getMarginEnd();

        TestSearchViewProviderFragment searchViewProviderFragment =
                new TestSearchViewProviderFragment();
        mMultiColumnSettings
                .getChildFragmentManager()
                .beginTransaction()
                .replace(R.id.preferences_detail, searchViewProviderFragment)
                .commitNow();
        updater.onTitleUpdated();

        var paramsWithSearch = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        int marginEndWithSearch = paramsWithSearch.getMarginEnd();

        int minTouchTargetPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        int iconWidthPx = searchButton.getDrawable().getIntrinsicWidth();
        int expectedOffset = (minTouchTargetPx - iconWidthPx) / 2;

        // Verify that the title scroll view end margin is shifted right by expectedOffset when the
        // search button is shown.
        assertEquals(marginEndWithoutSearch - expectedOffset, marginEndWithSearch);
    }

    /** Regression test for incorrect title layout after display rotation. crbug.com/541103334 */
    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testMaybeUpdateMargins_usesDetailViewWidthWhenRecyclerViewNotLaidOut() {
        FrameLayout detailView = new FrameLayout(mActivity);
        detailView.layout(0, 0, 1000, 100);
        // Add a recycler view that has width = 0 (not laid out yet).
        View recyclerView = new View(mActivity);
        recyclerView.setId(R.id.recycler_view);
        detailView.addView(recyclerView);
        mMultiColumnSettings.setDetailView(detailView);

        RelativeLayout rootLayout = new RelativeLayout(mActivity);
        HorizontalScrollView titleScrollView = new HorizontalScrollView(mActivity);
        RelativeLayout.LayoutParams scrollParams =
                new RelativeLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rootLayout.addView(titleScrollView, scrollParams);
        titleScrollView.addView(mContainer);

        List<MultiColumnSettings.Title> titles = new ArrayList<>();
        titles.add(
                new MultiColumnSettings.Title("uuid1", createTitleSupplier("Appearance"), 0, null));
        mMultiColumnSettings.setFakeTitles(titles);

        MultiColumnTitleUpdater updater = createMultiColumnTitleUpdater();
        updater.onTitleUpdated();

        var params = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        int marginStart = params.getMarginStart();

        // Verify that start margin was updated based on detailView's width (1000px) even though
        // recyclerView's width is 0.
        assertTrue(marginStart > 0);
    }
}
