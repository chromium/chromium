// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentHostCallback;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MultiColumnSettings;
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
        // Avoid runnable pollution between tests.
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testAccessibilityStateChanged_whenMultiColumnSettingsNotAdded_doesNotCrash() {
        // Mock multiColumnSettings to return null context (not attached).
        when(mMultiColumnSettings.getContext()).thenReturn(null);

        var state =
                new AccessibilityState.State(
                        false, false, false, false, false, false, false, false, false);

        // This call should not crash.
        mCoordinator.onAccessibilityStateChanged(state, state);
    }

    @Test
    public void testAccessibilityStateChanged_whenMultiColumnSettingsAdded_doesNotCrash()
            throws Exception {
        // Mock multiColumnSettings to be attached (getContext() is non-null) and return a child
        // fragment manager.
        when(mMultiColumnSettings.getContext()).thenReturn(mActivity);

        // Set mHost to a non-null mock to bypass mHost != null check in getChildFragmentManager()
        Object mockHost = mock(FragmentHostCallback.class);
        setFragmentField(mMultiColumnSettings, "mHost", mockHost);

        // Set mChildFragmentManager to our mocked childFragmentManager
        FragmentManager childFragmentManager = mock(FragmentManager.class);
        setFragmentField(mMultiColumnSettings, "mChildFragmentManager", childFragmentManager);
        when(childFragmentManager.isStateSaved()).thenReturn(false);

        var state =
                new AccessibilityState.State(
                        false, false, false, false, false, false, false, false, false);

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
    public void testOnHeaderLayoutUpdated_switchesToSingleColumnMode() throws Exception {
        when(mMultiColumnSettings.getContext()).thenReturn(mActivity);
        Object mockHost = mock(FragmentHostCallback.class);
        setFragmentField(mMultiColumnSettings, "mHost", mockHost);
        FragmentManager childFragmentManager = mock(FragmentManager.class);
        setFragmentField(mMultiColumnSettings, "mChildFragmentManager", childFragmentManager);

        SlidingPaneLayout slidingPaneLayout = new SlidingPaneLayout(mActivity);
        when(mMultiColumnSettings.getView()).thenReturn(slidingPaneLayout);
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

    private void setFragmentField(Fragment fragment, String fieldName, Object value)
            throws Exception {
        java.lang.reflect.Field field = Fragment.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        field.set(fragment, value);
    }
}
