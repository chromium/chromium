// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

import java.util.ArrayList;
import java.util.List;

@Batch(Batch.UNIT_TESTS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class MultiColumnSettingsUnitTest {
    // Hack to trick the test target about back stack entries.
    // In this test, count is enough.
    private static class TestFragmentManager extends FragmentManager {
        private int mBackStackCount;

        void addBackStack() {
            ++mBackStackCount;
        }

        void removeBackStack() {
            ++mBackStackCount;
        }

        void clearBackStack() {
            mBackStackCount = 0;
        }

        @Override
        public int getBackStackEntryCount() {
            return mBackStackCount;
        }
    }

    // Stub fragment instance of EmbeddableSettingsPage providing a fake page title instance.
    private static class TestFragment extends Fragment implements EmbeddableSettingsPage {
        private final ObservableSupplier<String> mTitleSupplier = new ObservableSupplierImpl();

        @Override
        public ObservableSupplier getPageTitle() {
            return mTitleSupplier;
        }

        @Override
        public @AnimationType int getAnimationType() {
            return AnimationType.PROPERTY;
        }
    }

    // Creation of fragments (specifically, ObservableSupplierImpl) requires
    // to run on non instrumentation thread.
    @Test
    @SmallTest
    @UiThreadTest
    public void testFragmentTracker() {
        List<MultiColumnSettings.Observer> observers = new ArrayList<>();
        var fragmentManager = new TestFragmentManager();

        var fragmentTracker = new MultiColumnSettings.FragmentTracker(observers);
        assertEquals(0, fragmentTracker.mTitles.size());

        // Simulate loading the main page at the left pane.
        var mainFragment = new MainSettings();
        fragmentTracker.onFragmentResumed(fragmentManager, mainFragment);
        // Detailed paget title should be not affected.
        assertEquals(0, fragmentTracker.mTitles.size());

        // Load a detailed page.
        var fragment1 = new TestFragment();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment1);

        assertEquals(1, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);
        }

        // Load another detailed page.
        var fragment2 = new TestFragment();
        fragmentManager.addBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment2);

        assertEquals(2, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);
        }

        // Load yet another detailed page.
        var fragment3 = new TestFragment();
        fragmentManager.addBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment3);

        assertEquals(3, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);

            var title3 = fragmentTracker.mTitles.get(2);
            assertSame(fragment3.getPageTitle(), title3.titleSupplier);
            assertEquals(2, title3.backStackCount);
        }

        // Restart the second fragment. The stack should be shrunk.
        fragmentManager.removeBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment2);

        assertEquals(2, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment1.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);

            var title2 = fragmentTracker.mTitles.get(1);
            assertSame(fragment2.getPageTitle(), title2.titleSupplier);
            assertEquals(1, title2.backStackCount);
        }

        // Emulation of the tap on a menu item in the main menu.
        var fragment4 = new TestFragment();
        fragmentManager.clearBackStack();
        fragmentTracker.onFragmentResumed(fragmentManager, fragment4);

        assertEquals(1, fragmentTracker.mTitles.size());
        {
            var title1 = fragmentTracker.mTitles.get(0);
            assertSame(fragment4.getPageTitle(), title1.titleSupplier);
            assertEquals(0, title1.backStackCount);
        }
    }
}
