// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityLocationBarLayout;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.ui.base.PageTransition;
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
    @Captor private ArgumentCaptor<OverrideUrlLoadingDelegate> mOverrideUrlLoadingDelegateCaptor;
    @Captor private ArgumentCaptor<Callback<String>> mBringTabGroupToFrontCallbackCaptor;

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
        verifySearchUiCoordinatorInitialized();
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
    public void testPanelEventsConsumed() {
        showOverlay();
        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);

        // Verify Touch event is consumed
        MotionEvent touchEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0f, 0f, 0);
        assertTrue(panelView.dispatchTouchEvent(touchEvent));

        // Verify Hover event is consumed
        MotionEvent hoverEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        assertTrue(panelView.dispatchGenericMotionEvent(hoverEvent));

        // Verify Generic Motion event is consumed
        MotionEvent motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_SCROLL, 0f, 0f, 0);
        assertTrue(panelView.dispatchGenericMotionEvent(motionEvent));

        // Verify Context Click is consumed (requires API 23+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            assertTrue(panelView.performContextClick());
        }
    }

    @Test
    public void testHide_hidesOverlayAndClearsFocus() {
        showOverlay();
        mCoordinator.hide();
        assertOverlayHidden();
    }

    @Test
    public void testLoadUrl_regular() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        OverrideUrlLoadingDelegate delegate = mOverrideUrlLoadingDelegateCaptor.getValue();
        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(
                                "https://www.google.com/search?q=test", PageTransition.TYPED)
                        .build();
        boolean handled = delegate.willHandleLoadUrlWithPostData(params, /* incognito= */ false);
        assertTrue(handled);
        assertFalse(mCoordinator.isVisible());

        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals("https://www.google.com/search?q=test", intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        assertFalse(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
    }

    @Test
    public void testLoadUrl_incognito() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        OverrideUrlLoadingDelegate delegate = mOverrideUrlLoadingDelegateCaptor.getValue();
        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(
                                "https://www.google.com/search?q=test", PageTransition.TYPED)
                        .build();
        boolean handled = delegate.willHandleLoadUrlWithPostData(params, /* incognito= */ true);
        assertTrue(handled);
        assertFalse(mCoordinator.isVisible());

        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals("https://www.google.com/search?q=test", intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertTrue(
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
    }

    @Test
    public void testBringTabGroupToFront() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        Callback<String> callback = mBringTabGroupToFrontCallbackCaptor.getValue();
        assertNotNull(callback);

        callback.onResult("group_id_1");
        assertFalse(mCoordinator.isVisible());

        Intent intent =
                Shadows.shadowOf(org.robolectric.RuntimeEnvironment.getApplication())
                        .getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals("group_id_1", IntentHandler.getBringTabGroupToFrontId(intent));
        assertEquals(
                2, intent.getIntExtra(IntentHandler.BRING_TAB_GROUP_TO_FRONT_SOURCE_EXTRA, -1));
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

    private void verifySearchUiCoordinatorInitialized() {
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
                        mOverrideUrlLoadingDelegateCaptor.capture(),
                        any(),
                        mBringTabGroupToFrontCallbackCaptor.capture(),
                        any(),
                        any(),
                        any(),
                        any());
    }
}
