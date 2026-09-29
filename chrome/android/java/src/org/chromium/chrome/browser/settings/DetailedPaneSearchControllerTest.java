// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link DetailedPaneSearchController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
public class DetailedPaneSearchControllerTest {

    /** Fake PreferenceFragment for testing. */
    public static class FakePreferenceFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getContext()));
        }
    }

    /** Fake MultiColumnSettings for testing without inflating MainSettings. */
    @SuppressWarnings("MissingSuperCall")
    public static class FakeMultiColumnSettings extends MultiColumnSettings {
        private View mDetailView;

        void setDetailView(View detailView) {
            mDetailView = detailView;
        }

        @Override
        public View getDetailView() {
            return mDetailView != null ? mDetailView : super.getDetailView();
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

    /** Fake SearchViewProvider fragment for testing. */
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private FakeMultiColumnSettings mMultiColumnSettings;
    private LinearLayout mContainer;
    private TextView mTitleView;
    private AtomicInteger mVisibilityChangeCount;
    private DetailedPaneSearchController mController;

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> mActivity = (TestActivity) activity);
        mContainer = new LinearLayout(mActivity);
        mTitleView = new TextView(mActivity);
        mTitleView.setText("Test Title");
        mContainer.addView(mTitleView);

        mMultiColumnSettings = new FakeMultiColumnSettings();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mMultiColumnSettings, "settings")
                .commitNow();

        mVisibilityChangeCount = new AtomicInteger(0);
        mController =
                new DetailedPaneSearchController(
                        mActivity,
                        mContainer,
                        mMultiColumnSettings,
                        mVisibilityChangeCount::incrementAndGet);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnTitlesUpdated_fragmentNotSearchViewProvider() {
        Fragment regularFragment = new Fragment();
        mController.onTitlesUpdated(mTitleView, regularFragment);

        assertFalse(mController.hasSearchButton());
        assertFalse(mController.isSearchOpen());
        assertEquals(1, mContainer.getChildCount());
        assertEquals(0, mController.getSearchButtonOffsetPx());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnTitlesUpdated_nullFragment() {
        mController.onTitlesUpdated(mTitleView, null);

        assertFalse(mController.hasSearchButton());
        assertFalse(mController.isSearchOpen());
        assertEquals(1, mContainer.getChildCount());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnTitlesUpdated_fragmentIsSearchViewProvider() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        assertTrue(mController.hasSearchButton());
        assertEquals(3, mContainer.getChildCount());

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        assertNotNull(searchButton);
        assertEquals(View.VISIBLE, searchButton.getVisibility());

        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchView);
        assertEquals(View.GONE, searchView.getVisibility());
        assertNull(searchView.getBackground());

        View searchPlate = searchView.findViewById(R.id.search_plate);
        assertNotNull(searchPlate);
        assertNull(searchPlate.getBackground());
        assertEquals(mActivity.getString(R.string.search), searchView.getQueryHint());

        var titleParams = (LinearLayout.LayoutParams) mTitleView.getLayoutParams();
        assertEquals(1f, titleParams.weight, 0.01f);
        assertEquals(fragment.getSearchView(), searchView);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOpenAndCloseSearch() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        assertNotNull(searchView);

        // Open search.
        searchButton.performClick();
        assertTrue(mController.isSearchOpen());
        assertEquals(View.GONE, mTitleView.getVisibility());
        assertEquals(View.GONE, searchButton.getVisibility());
        assertEquals(View.VISIBLE, searchView.getVisibility());
        assertTrue(mVisibilityChangeCount.get() > 0);

        // Close search.
        int countBeforeClose = mVisibilityChangeCount.get();
        mController.closeSearch();
        assertFalse(mController.isSearchOpen());
        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(countBeforeClose + 1, mVisibilityChangeCount.get());
        assertFalse(mController.handleBackAction());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testHandleBackAction() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        // When search is not open, handleBackAction() returns false.
        assertFalse(mController.isSearchOpen());
        assertFalse(mController.handleBackAction());

        // Open search.
        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        searchButton.performClick();
        assertTrue(mController.isSearchOpen());

        // When search is open, handleBackAction() closes search and returns true.
        assertTrue(mController.handleBackAction());
        assertFalse(mController.isSearchOpen());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnBackPressedDispatcher_closesSearch() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        assertNotNull(searchView);

        searchButton.performClick();
        assertTrue(mController.isSearchOpen());

        // Pressing back on activity dispatcher triggers callback and closes search.
        mActivity.getOnBackPressedDispatcher().onBackPressed();
        assertFalse(mController.isSearchOpen());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testEscapeKey_closesSearch() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);
        mActivity.setContentView(mContainer);

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        assertNotNull(searchView);

        searchButton.performClick();
        assertTrue(mController.isSearchOpen());

        View searchSrcTextView = searchView.findViewById(R.id.search_src_text);
        assertNotNull(searchSrcTextView);

        KeyEvent escapeDown = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
        assertTrue(searchSrcTextView.dispatchKeyEvent(escapeDown));
        assertFalse(mController.isSearchOpen());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testObserver_updatesSearchClosed() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        assertNotNull(searchView);

        searchButton.performClick();
        assertTrue(mController.isSearchOpen());
        assertNotNull(fragment.getObserver());

        // Notify observer that search closed.
        fragment.getObserver().onUpdated(false);
        assertFalse(mController.isSearchOpen());
        assertEquals(View.GONE, searchView.getVisibility());
        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals(View.VISIBLE, searchButton.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testReset_cleansUpViewsAndState() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        ChromeImageButton searchButton = (ChromeImageButton) mContainer.getChildAt(1);
        SearchView searchView = (SearchView) mContainer.getChildAt(2);
        assertNotNull(searchButton);
        assertNotNull(searchView);

        searchButton.performClick();
        assertTrue(mController.isSearchOpen());

        mController.reset();

        assertFalse(mController.isSearchOpen());
        assertFalse(mController.hasSearchButton());
        assertEquals(1, mContainer.getChildCount());
        assertFalse(mController.handleBackAction());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testGetSearchButtonOffsetPx() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mController.onTitlesUpdated(mTitleView, fragment);

        assertTrue(mController.hasSearchButton());
        int offset = mController.getSearchButtonOffsetPx();
        assertTrue(offset >= 0);

        mController.reset();
        assertEquals(0, mController.getSearchButtonOffsetPx());
    }
}
