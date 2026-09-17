// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;

import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.Fragment;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
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

    public static class TestBaseSiteSettingsFragment extends BaseSiteSettingsFragment {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {}
    }

    @Before
    public void setUp() {
        when(mSearchCoordinatorSupplier.get()).thenReturn(mSearchCoordinator);
    }

    private FragmentDependencyProvider createProvider(boolean shownInTab) {
        return new FragmentDependencyProvider(
                mActivity,
                shownInTab,
                mProfile,
                new OneshotSupplierImpl<>(),
                mActivityResultTracker,
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                ObservableSuppliers.createMonotonic(),
                mSearchCoordinatorSupplier);
    }

    @Test
    public void testOnFragmentAttached_SearchViewProvider_shownInTab() {
        FragmentDependencyProvider provider = createProvider(/* shownInTab= */ true);
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        provider.onFragmentAttached(null, fragment, null);

        // Settings shown in a tab keeps the main search bar visible, so no observer is installed.
        assertNull(fragment.getObserver());
    }

    @Test
    public void testOnFragmentAttached_SearchViewProvider_notShownInTab() {
        FragmentDependencyProvider provider = createProvider(/* shownInTab= */ false);
        TestSearchViewProviderFragment fragment = new TestSearchViewProviderFragment();
        provider.onFragmentAttached(null, fragment, null);

        assertNotNull(fragment.getObserver());
    }

    @Test
    public void testAttachDependencies_BaseSiteSettingsFragment_canBeCalledMultipleTimes() {
        FragmentDependencyProvider provider = createProvider(/* shownInTab= */ false);
        TestBaseSiteSettingsFragment fragment = new TestBaseSiteSettingsFragment();
        provider.attachDependencies(null, fragment);
        assertNotNull(fragment.getSiteSettingsDelegate());

        // Attaching dependencies again (e.g. during Activity recreation / SettingsInTab init)
        // should update the delegate without throwing an AssertionError.
        provider.attachDependencies(null, fragment);
        assertNotNull(fragment.getSiteSettingsDelegate());
    }
}
