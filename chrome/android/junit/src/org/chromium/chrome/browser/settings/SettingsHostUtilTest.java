// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SettingsHostUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsHostUtilTest {
    /** Activity that hosts settings in a tab, like ChromeTabbedActivity does. */
    public static class TabHostActivity extends FragmentActivity implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return true;
        }
    }

    /** Activity that hosts settings itself, like SettingsActivity does. */
    public static class ActivityHost extends FragmentActivity implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return false;
        }
    }

    /** Fragment that hosts settings in a tab, like SettingsHostFragment does. */
    public static class HostFragment extends Fragment implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return true;
        }
    }

    private FragmentActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FragmentActivity.class).setup().get();
    }

    private void attach(Fragment fragment) {
        mActivity.getSupportFragmentManager().beginTransaction().add(fragment, null).commitNow();
    }

    private void attachChild(Fragment parent, Fragment child) {
        parent.getChildFragmentManager().beginTransaction().add(child, null).commitNow();
    }

    @Test
    public void testNoHost_ReturnsFalse() {
        // Fragments hosted by a plain activity, e.g. in tests, behave like settings in an activity.
        Fragment fragment = new Fragment();
        attach(fragment);
        assertFalse(SettingsHostUtil.isShownInTab(fragment));
    }

    @Test
    public void testActivityHost() {
        FragmentActivity activity = Robolectric.buildActivity(ActivityHost.class).setup().get();
        Fragment fragment = new Fragment();
        activity.getSupportFragmentManager().beginTransaction().add(fragment, null).commitNow();
        assertFalse(SettingsHostUtil.isShownInTab(fragment));
    }

    @Test
    public void testTabHostActivity() {
        FragmentActivity activity = Robolectric.buildActivity(TabHostActivity.class).setup().get();
        Fragment fragment = new Fragment();
        activity.getSupportFragmentManager().beginTransaction().add(fragment, null).commitNow();
        assertTrue(SettingsHostUtil.isShownInTab(fragment));
    }

    @Test
    public void testHostFragment_IsItsOwnHost() {
        HostFragment host = new HostFragment();
        attach(host);
        assertTrue(SettingsHostUtil.isShownInTab(host));
    }

    @Test
    public void testNestedFragments_FindHostFragment() {
        // Leaf settings fragments are nested several levels below the host fragment, e.g.
        // SettingsHostFragment > MultiColumnSettings > a leaf preference fragment.
        HostFragment host = new HostFragment();
        attach(host);
        Fragment middle = new Fragment();
        attachChild(host, middle);
        Fragment leaf = new Fragment();
        attachChild(middle, leaf);
        assertTrue(SettingsHostUtil.isShownInTab(leaf));
    }

    @Test
    public void testHostFragmentWins_OverActivity() {
        // The activity hosting a settings tab also hosts the rest of the browser UI, so the host
        // fragment is the more specific answer.
        FragmentActivity activity = Robolectric.buildActivity(ActivityHost.class).setup().get();
        HostFragment host = new HostFragment();
        activity.getSupportFragmentManager().beginTransaction().add(host, null).commitNow();
        Fragment leaf = new Fragment();
        attachChild(host, leaf);
        assertTrue(SettingsHostUtil.isShownInTab(leaf));
    }

    @Test
    public void testAttachContext_UsedWhenFragmentNotAttached() {
        // Fragment.getActivity() returns null during onAttach(), so the context must be used.
        FragmentActivity activity = Robolectric.buildActivity(TabHostActivity.class).setup().get();
        Fragment fragment = new Fragment();
        assertFalse(SettingsHostUtil.isShownInTab(fragment));
        assertTrue(SettingsHostUtil.isShownInTab(fragment, activity));
    }
}
