// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoPreferenceSubpageController;

/** Tests for PageInfoPreferenceSubpageController. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageInfoPreferenceSubpageControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock private PageInfoControllerDelegate mDelegate;
    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;
    @Mock private FragmentManager mFragmentManager;
    @Mock private FragmentTransaction mFragmentTransaction;
    @Mock private BaseSiteSettingsFragment mFragment;
    @Mock private View mView;

    private static class TestPreferenceSubpageController
            extends PageInfoPreferenceSubpageController {
        public TestPreferenceSubpageController(PageInfoControllerDelegate delegate) {
            super(delegate);
        }

        @Override
        public String getSubpageTitle() {
            return "Test Title";
        }

        @Override
        public @Nullable View createViewForSubpage(ViewGroup parent) {
            return null;
        }

        @Override
        public void onSubpageRemoved() {
            removeSubpageFragment();
        }

        @Override
        public void clearData() {}

        @Override
        public void updateRowIfNeeded() {}

        @Override
        public void updateSubpageIfNeeded() {}

        public @Nullable View addSubpage(BaseSiteSettingsFragment fragment) {
            return addSubpageFragment(fragment);
        }

        public void removeSubpage() {
            removeSubpageFragment();
        }

        public boolean canCreate() {
            return canCreateSubpageFragment();
        }
    }

    private TestPreferenceSubpageController mController;

    @Before
    public void setUp() {
        when(mDelegate.getFragmentManager()).thenReturn(mFragmentManager);
        when(mDelegate.getSiteSettingsDelegate()).thenReturn(mSiteSettingsDelegate);
        when(mFragmentManager.beginTransaction()).thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.add(any(), any())).thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.remove(any())).thenReturn(mFragmentTransaction);
        when(mFragment.getView()).thenReturn(mView);

        mController = new TestPreferenceSubpageController(mDelegate);
    }

    @Test
    public void testRemoveSubpageFragment_WhenSubPageIsNull_DoesNotThrowOrCallRemove() {
        // Initially mSubPage is null.
        mController.removeSubpage();

        verify(mFragmentManager, never()).beginTransaction();
    }

    @Test
    @DisabledTest(message = "b/552459911")
    public void testRemoveSubpageFragment_Twice_IsIdempotent() {
        // Add subpage first.
        View view = mController.addSubpage(mFragment);
        assertEquals(mView, view);
        assertEquals(mView, mController.getCurrentSubpageView());

        // First remove should perform transaction.
        mController.removeSubpage();
        verify(mFragmentTransaction).remove(mFragment);
        verify(mFragmentTransaction, times(2)).commitNow();
        assertNull(mController.getCurrentSubpageView());

        // Second remove should be a safe no-op.
        mController.removeSubpage();
        verify(mFragmentTransaction).remove(mFragment);
        verify(mFragmentTransaction, times(2)).commitNow();
    }

    @Test
    public void testRemoveSubpageFragment_WhenFragmentManagerIsNull_DoesNotCrash() {
        mController.addSubpage(mFragment);
        when(mDelegate.getFragmentManager()).thenReturn(null);

        mController.removeSubpage();
        assertNull(mController.getCurrentSubpageView());
    }

    @Test
    public void testRemoveSubpageFragment_WhenStateSaved_DoesNotCommitTransaction() {
        mController.addSubpage(mFragment);
        when(mFragmentManager.isStateSaved()).thenReturn(true);

        mController.removeSubpage();
        verify(mFragmentTransaction, never()).remove(any());
        assertNull(mController.getCurrentSubpageView());
    }

    @Test
    public void testRemoveSubpageFragment_WhenDestroyed_DoesNotCommitTransaction() {
        mController.addSubpage(mFragment);
        when(mFragmentManager.isDestroyed()).thenReturn(true);

        mController.removeSubpage();
        verify(mFragmentTransaction, never()).remove(any());
        assertNull(mController.getCurrentSubpageView());
    }

    @Test
    public void testAddSubpageFragment_WhenStateSaved_ReturnsNull() {
        when(mFragmentManager.isStateSaved()).thenReturn(true);

        View view = mController.addSubpage(mFragment);
        assertNull(view);
        assertNull(mController.getCurrentSubpageView());

        // Removing should also be safe and do nothing.
        mController.removeSubpage();
        verify(mFragmentTransaction, never()).remove(any());
    }

    @Test
    public void testAddSubpageFragment_WhenDestroyed_ReturnsNull() {
        when(mFragmentManager.isDestroyed()).thenReturn(true);

        View view = mController.addSubpage(mFragment);
        assertNull(view);
        assertNull(mController.getCurrentSubpageView());

        // Removing should also be safe and do nothing.
        mController.removeSubpage();
        verify(mFragmentTransaction, never()).remove(any());
    }

    @Test
    public void testCanCreateSubpageFragment() {
        assertTrue(mController.canCreate());

        when(mFragmentManager.isStateSaved()).thenReturn(true);
        assertFalse(mController.canCreate());

        when(mFragmentManager.isStateSaved()).thenReturn(false);
        when(mFragmentManager.isDestroyed()).thenReturn(true);
        assertFalse(mController.canCreate());

        when(mDelegate.getFragmentManager()).thenReturn(null);
        assertFalse(mController.canCreate());
    }
}
