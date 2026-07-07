// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityLocationBarLayout;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link TabSearchOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSearchOverlayCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ViewGroup mParentContainer;
    private TabSearchOverlayCoordinator mCoordinator;
    private View mPanelContainer;
    private View mScrim;

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SearchUiCoordinator mSearchUiCoordinator;
    @Mock private LocationBarCoordinator mLocationBarCoordinator;
    @Mock private SearchActivityLocationBarLayout mSearchBox;
    @Mock private Profile mProfile;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;

    private final SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        mActivity = controller.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mParentContainer = new FrameLayout(mActivity);
        mActivity.setContentView(mParentContainer);

        mTabModelSelectorSupplier.set(mTabModelSelector);

        when(mSearchUiCoordinator.getLocationBarCoordinator()).thenReturn(mLocationBarCoordinator);
        when(mSearchUiCoordinator.getSearchBox()).thenReturn(mSearchBox);

        var profileSupplier = ObservableSuppliers.createMonotonic(mProfile);
        mCoordinator =
                new TabSearchOverlayCoordinator(
                        mActivity,
                        mParentContainer,
                        mWindowAndroid,
                        profileSupplier,
                        mSnackbarManager,
                        ObservableSuppliers.createNonNull(mModalDialogManager),
                        mActivityLifecycleDispatcher,
                        mTabModelSelectorSupplier,
                        /* edgeToEdgeSystemBarColorHelper= */ null);
        mCoordinator.setSearchUiCoordinatorForTesting(mSearchUiCoordinator);

        // Inflate the overlay and initialize member views.
        mCoordinator.ensureInitialized();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mPanelContainer = mParentContainer.findViewById(R.id.tab_search_overlay_container);
        mScrim = mParentContainer.findViewById(R.id.tab_search_overlay_scrim);

        // Clear mock invocations from setup phase to ensure test assertions are isolated.
        clearInvocations(mLocationBarCoordinator);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        assertNull(mCoordinator.getPanelContainerForTesting());
        verify(mSearchUiCoordinator).destroy();
    }

    @Test
    public void testInitialState() {
        assertFalse(mCoordinator.isVisible());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
    }

    @Test
    public void testShow_inflatesAndShowsOverlay() {
        showOverlay();

        // Verify SearchUiCoordinator is initialized and query is started
        verify(mSearchUiCoordinator)
                .initialize(
                        any(),
                        any(),
                        eq(mWindowAndroid),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(mTabModelSelectorSupplier),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any());
        verify(mSearchUiCoordinator)
                .setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);
        verify(mSearchUiCoordinator)
                .beginQuery(
                        eq(IntentOrigin.HUB), eq(SearchType.TEXT), eq(null), eq(mWindowAndroid));
    }

    @Test
    public void testClickScrim_hidesOverlay() {
        showOverlay();
        mScrim.performClick();
        assertOverlayHidden();
    }

    @Test
    public void testHide_hidesOverlayAndClearsFocus() {
        showOverlay();
        mCoordinator.hide();
        assertOverlayHidden();
    }

    private void showOverlay() {
        mCoordinator.show();
        assertOverlayShown();
    }

    private void assertOverlayShown() {
        assertTrue(mCoordinator.isVisible());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
    }

    private void assertOverlayHidden() {
        assertFalse(mCoordinator.isVisible());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        verify(mLocationBarCoordinator).clearOmniboxFocus();
    }
}
