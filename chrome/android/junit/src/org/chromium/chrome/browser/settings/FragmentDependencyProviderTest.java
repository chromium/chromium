// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.Fragment;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.ui.base.ActivityResultTracker;

import java.util.function.Supplier;

/** Tests for {@link FragmentDependencyProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FragmentDependencyProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private Supplier<SettingsSearchCoordinator> mSearchCoordinatorSupplier;
    @Mock private SettingsSearchCoordinator mSearchCoordinator;

    private FragmentDependencyProvider mProvider;

    public static class TestSearchViewProviderFragment extends Fragment
            implements SearchViewProvider {
        private SearchViewProvider.Observer mObserver;

        @Override
        public void setSearchViewObserver(SearchViewProvider.Observer observer) {
            mObserver = observer;
        }

        public SearchViewProvider.Observer getObserver() {
            return mObserver;
        }

        @Override
        public void initSearchView(SearchView searchView) {}
    }

    @Before
    public void setUp() {
        when(mSearchCoordinatorSupplier.get()).thenReturn(mSearchCoordinator);
        mProvider =
                new FragmentDependencyProvider(
                        mActivity,
                        mProfile,
                        new OneshotSupplierImpl<>(),
                        mActivityResultTracker,
                        new OneshotSupplierImpl<>(),
                        new OneshotSupplierImpl<>(),
                        ObservableSuppliers.createMonotonic(),
                        mSearchCoordinatorSupplier);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testOnFragmentAttached_SearchViewProvider_SettingsInTabEnabled() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mProvider.onFragmentAttached(null, fragment, null);

        assertNull(fragment.getObserver());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testOnFragmentAttached_SearchViewProvider_SettingsInTabDisabled() {
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        mProvider.onFragmentAttached(null, fragment, null);

        assertNotNull(fragment.getObserver());
    }
}
