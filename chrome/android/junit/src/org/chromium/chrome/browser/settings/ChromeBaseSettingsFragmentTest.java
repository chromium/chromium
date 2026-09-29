// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

/** Unit tests for {@link ChromeBaseSettingsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeBaseSettingsFragmentTest {
    /** Activity that hosts settings in a tab, like ChromeTabbedActivity does. */
    public static class TabHostActivity extends FragmentActivity implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return true;
        }
    }

    /** Fragment that hosts settings in a tab, like SettingsHostFragment does. */
    public static class HostFragment extends Fragment implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return true;
        }
    }

    /** Minimal concrete settings fragment. */
    public static class TestSettingsFragment extends ChromeBaseSettingsFragment {
        private final SettableMonotonicObservableSupplier<String> mPageTitle =
                ObservableSuppliers.createMonotonic();

        @Override
        public MonotonicObservableSupplier<String> getPageTitle() {
            return mPageTitle;
        }

        @Override
        public int getAnimationType() {
            return AnimationType.PROPERTY;
        }

        @Override
        public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {}

        @Override
        public @Nullable View onCreateView(
                LayoutInflater inflater,
                @Nullable ViewGroup container,
                @Nullable Bundle savedInstanceState) {
            // Skip PreferenceFragmentCompat's view inflation, which requires the host theme to
            // define preferenceTheme. These tests only care about the context from onAttach().
            return null;
        }
    }

    private static void attach(FragmentActivity activity, Fragment fragment) {
        activity.getSupportFragmentManager().beginTransaction().add(fragment, null).commitNow();
    }

    @Test
    public void testNotShownInTab_UsesHostContext() {
        // A plain activity host, e.g. SettingsActivity or a test activity, already applies the
        // settings theme itself, so the fragment must not wrap the context again.
        FragmentActivity activity = Robolectric.buildActivity(FragmentActivity.class).setup().get();
        TestSettingsFragment fragment = new TestSettingsFragment();
        attach(activity, fragment);

        assertSame(activity, fragment.getContext());
    }

    @Test
    public void testShownInTab_AppliesSettingsTheme() {
        FragmentActivity activity = Robolectric.buildActivity(TabHostActivity.class).setup().get();
        TestSettingsFragment fragment = new TestSettingsFragment();
        attach(activity, fragment);

        Context context = fragment.getContext();
        assertNotNull(context);
        assertEquals(R.style.ThemeOverlay_Chromium_Settings, context.getThemeResId());
    }

    @Test
    public void testShownInTab_FoundThroughParentFragment() {
        // Leaf settings fragments sit below the host fragment, and Fragment.getActivity() is null
        // during onAttach(), so the host must be found by walking up the fragment tree.
        FragmentActivity activity = Robolectric.buildActivity(FragmentActivity.class).setup().get();
        HostFragment host = new HostFragment();
        attach(activity, host);

        TestSettingsFragment fragment = new TestSettingsFragment();
        host.getChildFragmentManager().beginTransaction().add(fragment, null).commitNow();

        Context context = fragment.getContext();
        assertNotNull(context);
        assertEquals(R.style.ThemeOverlay_Chromium_Settings, context.getThemeResId());
    }

    @Test
    public void testShownInTab_TextViewDoesNotInheritScrollbars() {
        // PreferenceTheme must not define android:scrollbars="vertical" globally, which would cause
        // all child views (including TextView elements for preference titles and summaries) to
        // inherit vertical scrollbars and trigger transient scrollbar rendering during resize.
        FragmentActivity activity = Robolectric.buildActivity(TabHostActivity.class).setup().get();
        TestSettingsFragment fragment = new TestSettingsFragment();
        attach(activity, fragment);

        Context context = fragment.getContext();
        assertNotNull(context);

        TextView textView = new TextView(context);
        assertFalse(
                "TextView must not inherit vertical scrollbars from the settings theme",
                textView.isVerticalScrollBarEnabled());
    }

    @Test
    public void testPreferenceFragmentListStyle_EnablesVerticalScrollbars() {
        // SettingsFragmentList (referenced via preferenceFragmentListStyle) must explicitly enable
        // vertical scrollbars so the root RecyclerView retains intentional list scrolling.
        FragmentActivity activity = Robolectric.buildActivity(TabHostActivity.class).setup().get();
        TestSettingsFragment fragment = new TestSettingsFragment();
        attach(activity, fragment);

        Context context = fragment.getContext();
        assertNotNull(context);

        View listView = new View(context, null, R.attr.preferenceFragmentListStyle);
        assertTrue(
                "Views styled with preferenceFragmentListStyle must enable vertical scrollbars",
                listView.isVerticalScrollBarEnabled());
    }
}
