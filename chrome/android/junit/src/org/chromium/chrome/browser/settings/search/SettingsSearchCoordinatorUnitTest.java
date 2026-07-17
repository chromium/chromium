// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentHostCallback;
import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
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

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FragmentActivity.class).create().get();

        // Create a real Toolbar and a title inside it, so that ToolbarUtils.getTitleTextView() can
        // find it and not return null (which would crash in constructor's setFragmentState call).
        mToolbar = new Toolbar(mActivity);
        mToolbar.setId(org.chromium.chrome.R.id.action_bar);
        mToolbar.setTitle("Settings");
        TextView titleView = new TextView(mActivity);
        titleView.setText("Settings");
        mToolbar.addView(titleView);
        mActivity.setContentView(mToolbar);

        SettableMonotonicObservableSupplier<ModalDialogManager> modalDialogSupplier =
                ObservableSuppliers.createMonotonic();
        modalDialogSupplier.set(mModalDialogManager);

        mCoordinator =
                new SettingsSearchCoordinator(
                        mActivity,
                        mToolbar,
                        () -> true,
                        mMultiColumnSettings,
                        new HashMap<>(),
                        mProfile,
                        (index) -> {},
                        modalDialogSupplier);
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

    private void setFragmentField(Fragment fragment, String fieldName, Object value)
            throws Exception {
        java.lang.reflect.Field field = Fragment.class.getDeclaredField(fieldName);
        field.setAccessible(true);
        field.set(fragment, value);
    }
}
