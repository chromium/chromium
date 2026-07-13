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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link SettingsPageFragmentDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageFragmentDelegateImplTest {
    private static final int CONTAINER_ID = R.id.content;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeBaseAppCompatActivity mActivity;
    @Mock private FragmentManager mFragmentManager;
    @Mock private FragmentTransaction mFragmentTransaction;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ViewGroup mContainerView;
    @Mock private SettingsHostFragment mMockSettingsHostFragment;
    @Mock private MultiColumnSettings mMultiColumnSettings;
    @Mock private View mFragmentView;
    @Mock private LinearLayout mTitleContainer;

    private SettingsPageFragmentDelegateImpl mDelegate;
    private View mInflatedSettingsView;

    @Before
    public void setUp() {
        SettableMonotonicObservableSupplier<ModalDialogManager> modalDialogSupplier =
                ObservableSuppliers.createMonotonic();
        modalDialogSupplier.set(mModalDialogManager);
        when(mActivity.getModalDialogManagerSupplier()).thenReturn(modalDialogSupplier);

        when(mActivity.getSupportFragmentManager()).thenReturn(mFragmentManager);
        when(mFragmentManager.beginTransaction()).thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.add(anyInt(), any(Fragment.class), anyString()))
                .thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.remove(any(Fragment.class))).thenReturn(mFragmentTransaction);
        when(mContainerView.getId()).thenReturn(CONTAINER_ID);

        Answer<Void> captureSettingsView =
                (invocation) -> {
                    mInflatedSettingsView = invocation.getArgument(0);
                    return null;
                };
        doAnswer(captureSettingsView).when(mContainerView).addView(any(View.class));

        when(mActivity.findViewById(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int id = invocation.getArgument(0);
                            return mInflatedSettingsView != null
                                    ? mInflatedSettingsView.findViewById(id)
                                    : null;
                        });

        // Mock LayoutInflater with correct theme to support inflating settings_activity.
        Context context =
                new android.view.ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_Chromium_Settings);
        LayoutInflater layoutInflater = LayoutInflater.from(context);
        when(mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(layoutInflater);
        when(mActivity.getResources()).thenReturn(context.getResources());
        when(mActivity.getTheme()).thenReturn(context.getTheme());

        mDelegate =
                new SettingsPageFragmentDelegateImpl(
                        mActivity,
                        mProfile,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mSnackbarManager,
                        mBottomSheetController,
                        mModalDialogManager);
    }

    @Test
    public void testInitSettings_registersDependencyProviderAndAddsFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);

        mDelegate.initSettings(mContainerView);

        // Verify FragmentDependencyProvider registration.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        boolean foundDependencyProvider = false;
        boolean foundWideDisplayPaddingApplier = false;
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            if (callback instanceof FragmentDependencyProvider) {
                foundDependencyProvider = true;
            } else if (callback instanceof WideDisplayPaddingApplier) {
                foundWideDisplayPaddingApplier = true;
            }
        }
        assertTrue(
                "Lifecycle callbacks should include FragmentDependencyProvider",
                foundDependencyProvider);
        assertTrue(
                "Lifecycle callbacks should include WideDisplayPaddingApplier",
                foundWideDisplayPaddingApplier);

        // Verify fragment creation and addition.
        verify(mFragmentTransaction)
                .add(eq(CONTAINER_ID), any(SettingsHostFragment.class), eq("settings_native_page"));
        verify(mFragmentTransaction).commitAllowingStateLoss();
    }

    @Test
    public void testInitSettings_removesSheetAndDialogContainers() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);

        mDelegate.initSettings(mContainerView);

        assertNotNull(mInflatedSettingsView);

        // Settings in a tab uses the bottom sheet and dialog containers from ChromeTabbedActivity,
        // not its own.
        assertNull(mInflatedSettingsView.findViewById(R.id.sheet_container));
        assertNull(mInflatedSettingsView.findViewById(R.id.dialog_container));
    }

    @Test
    public void testInitSettings_reusesExistingFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);

        mDelegate.initSettings(mContainerView);

        // Verify we registered the callback but did NOT add a new fragment
        verify(mFragmentManager, atLeastOnce()).registerFragmentLifecycleCallbacks(any(), eq(true));
        verify(mFragmentTransaction, never()).add(anyInt(), any(), anyString());
    }

    @Test
    public void testDestroySettings_unregistersCallbacksAndRemovesFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);

        // Initialize first so the delegate has callbacks and fragment references.
        mDelegate.initSettings(mContainerView);

        // Retrieve the registered callbacks to verify they get unregistered.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));

        mDelegate.destroySettings();

        // Verify unregistration for all registered callbacks.
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            verify(mFragmentManager).unregisterFragmentLifecycleCallbacks(callback);
        }

        // Verify fragment removal.
        verify(mFragmentTransaction).remove(any(SettingsHostFragment.class));

        // Verify commits, 1 for init, 1 for destroy.
        verify(mFragmentTransaction, Mockito.times(2)).commitAllowingStateLoss();
    }

    @Test
    public void testGetMainFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);
        mDelegate.initSettings(mContainerView);

        Fragment mockFragment = mock(Fragment.class);
        when(mMockSettingsHostFragment.isAttachedToActivity()).thenReturn(true);
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(mockFragment);

        assertEquals(mockFragment, mDelegate.getMainFragment());
    }

    @Test
    public void testGetMultiColumnSettings() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);
        mDelegate.initSettings(mContainerView);

        when(mMockSettingsHostFragment.isAttachedToActivity()).thenReturn(true);
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(mMultiColumnSettings);

        assertEquals(mMultiColumnSettings, mDelegate.getMultiColumnSettings());
    }

    @Test
    public void testGetHelpAndFeedbackLauncher() {
        HelpAndFeedbackLauncher launcher = mDelegate.getHelpAndFeedbackLauncher();
        assertNotNull(launcher);
        assertTrue(launcher instanceof HelpAndFeedbackLauncherImpl);
    }

    @Test
    public void testInitSettings_createsTitleUpdater() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);
        mDelegate.initSettings(mContainerView);

        // Capture all registered FragmentLifecycleCallbacks.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));

        when(mFragmentView.findViewById(R.id.settings_title_in_detailed_pane))
                .thenReturn(mTitleContainer);

        // Run the view creation callback for all registered callbacks.
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            callback.onFragmentViewCreated(
                    mFragmentManager, mMultiColumnSettings, mFragmentView, null);
        }

        // Verify that the title updater was created and registered as an observer of
        // MultiColumnSettings.
        ArgumentCaptor<MultiColumnTitleUpdater> observerCaptor =
                ArgumentCaptor.forClass(MultiColumnTitleUpdater.class);
        verify(mMultiColumnSettings).addObserver(observerCaptor.capture());
        MultiColumnTitleUpdater titleUpdater = observerCaptor.getValue();
        assertNotNull(titleUpdater);

        // Observer removal is tested in the next test.
    }

    @Test
    public void testDestroySettings_destroysTitleUpdater() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);
        mDelegate.initSettings(mContainerView);

        // Capture lifecycle callbacks.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));

        // Set up mocks.
        when(mFragmentView.findViewById(R.id.settings_title_in_detailed_pane))
                .thenReturn(mTitleContainer);
        when(mMockSettingsHostFragment.isAttachedToActivity()).thenReturn(true);
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(mMultiColumnSettings);

        // Trigger view creation.
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            callback.onFragmentViewCreated(
                    mFragmentManager, mMultiColumnSettings, mFragmentView, null);
        }

        ArgumentCaptor<MultiColumnTitleUpdater> observerCaptor =
                ArgumentCaptor.forClass(MultiColumnTitleUpdater.class);
        verify(mMultiColumnSettings).addObserver(observerCaptor.capture());
        MultiColumnTitleUpdater titleUpdater = observerCaptor.getValue();
        assertNotNull(titleUpdater);

        // Destroy settings.
        mDelegate.destroySettings();

        // Verify that the observer was removed.
        verify(mMultiColumnSettings).removeObserver(titleUpdater);
    }

    @Test
    public void testIsTwoColumnSettingsVisible() {
        // Setup mSettingsHostFragment.
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);
        mDelegate.initSettings(mContainerView);
        when(mMockSettingsHostFragment.isAttachedToActivity()).thenReturn(true);

        // Case 1: getMultiColumnSettings() is null.
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(null);
        assertFalse(mDelegate.isTwoColumnSettingsVisible());

        // Case 2: getMultiColumnSettings() is non-null and not two column.
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(mMultiColumnSettings);
        when(mMultiColumnSettings.isTwoColumn()).thenReturn(false);
        assertFalse(mDelegate.isTwoColumnSettingsVisible());

        // Case 3: getMultiColumnSettings() is non-null and two column.
        when(mMultiColumnSettings.isTwoColumn()).thenReturn(true);
        assertTrue(mDelegate.isTwoColumnSettingsVisible());
    }

    @Test
    public void testInitSettings_createsSearchCoordinator() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);
        mDelegate.initSettings(mContainerView);

        // Capture all registered FragmentLifecycleCallbacks.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));

        when(mFragmentView.findViewById(R.id.settings_title_in_detailed_pane))
                .thenReturn(mTitleContainer);

        // Run the view creation callback for all registered callbacks.
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            callback.onFragmentViewCreated(
                    mFragmentManager, mMultiColumnSettings, mFragmentView, null);
        }

        // Verify that the search coordinator was created and registered as an observer of
        // MultiColumnSettings.
        SettingsSearchCoordinator searchCoordinator = mDelegate.getSearchCoordinator();
        assertNotNull(searchCoordinator);
        verify(mMultiColumnSettings).addObserver(searchCoordinator);
    }

    @Test
    public void testDestroySettings_destroysSearchCoordinator() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);
        mDelegate.initSettings(mContainerView);

        // Capture lifecycle callbacks.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager, atLeastOnce())
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));

        // Set up mocks.
        when(mFragmentView.findViewById(R.id.settings_title_in_detailed_pane))
                .thenReturn(mTitleContainer);
        when(mMockSettingsHostFragment.isAttachedToActivity()).thenReturn(true);
        when(mMockSettingsHostFragment.getActiveFragment()).thenReturn(mMultiColumnSettings);

        // Trigger view creation.
        for (FragmentManager.FragmentLifecycleCallbacks callback : callbackCaptor.getAllValues()) {
            callback.onFragmentViewCreated(
                    mFragmentManager, mMultiColumnSettings, mFragmentView, null);
        }

        SettingsSearchCoordinator searchCoordinator = mDelegate.getSearchCoordinator();
        assertNotNull(searchCoordinator);

        // Destroy settings.
        mDelegate.destroySettings();

        // Verify that the observer was removed.
        verify(mMultiColumnSettings).removeObserver(searchCoordinator);
    }
}
