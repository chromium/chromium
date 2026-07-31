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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityLocationBarLayout;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.concurrent.TimeUnit;

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
    @Mock private UrlBarCoordinator mUrlBarCoordinator;
    @Mock private SearchActivityLocationBarLayout mSearchBox;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BackPressManager mBackPressManager;
    @Mock private CompositorViewHolder mCompositorViewHolder;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;

    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();

    private final SettableNonNullObservableSupplier<Boolean> mSuggestionsListNonEmptySupplier =
            ObservableSuppliers.createNonNull(false);

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createMonotonic();

    @Captor private ArgumentCaptor<OverrideUrlLoadingDelegate> mOverrideUrlLoadingDelegateCaptor;
    @Captor private ArgumentCaptor<Callback<String>> mBringTabGroupToFrontCallbackCaptor;

    @Before
    @SuppressWarnings("unchecked")
    public void setUp() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        mActivity = controller.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mParentContainer = new FrameLayout(mActivity);
        mActivity.setContentView(mParentContainer);

        mTabModelSelectorSupplier.set(mTabModelSelector);
        mProfileSupplier.set(mProfile);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);

        when(mSearchUiCoordinator.getLocationBarCoordinator()).thenReturn(mLocationBarCoordinator);
        when(mLocationBarCoordinator.getUrlBarCoordinator()).thenReturn(mUrlBarCoordinator);
        when(mSearchUiCoordinator.getSearchBox()).thenReturn(mSearchBox);
        when(mLocationBarCoordinator.getSuggestionsListNonEmptySupplier())
                .thenReturn(mSuggestionsListNonEmptySupplier);

        mCoordinator =
                new TabSearchOverlayCoordinator(
                        mActivity,
                        mParentContainer,
                        mWindowAndroid,
                        mProfileSupplier,
                        mSnackbarManager,
                        ObservableSuppliers.createNonNull(mModalDialogManager),
                        mActivityLifecycleDispatcher,
                        mTabModelSelectorSupplier,
                        /* edgeToEdgeSystemBarColorHelper= */ null,
                        mBackPressManager,
                        ObservableSuppliers.createNonNull(mCompositorViewHolder),
                        mTabGroupUiActionHandlerSupplier);
        mCoordinator.setSearchUiCoordinatorForTesting(mSearchUiCoordinator);

        // Inflate the overlay and initialize member views.
        mCoordinator.ensureInitialized();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mPanelContainer = mParentContainer.findViewById(R.id.tab_search_overlay_container);
        mScrim = mParentContainer.findViewById(R.id.tab_search_overlay_scrim);

        assertTrue(mSuggestionsListNonEmptySupplier.hasObservers());

        verify(mBackPressManager)
                .addHandler(mCoordinator, BackPressHandler.Type.TAB_SEARCH_OVERLAY);

        // Clear mock invocations from setup phase to ensure test assertions are isolated.
        clearInvocations(mLocationBarCoordinator);
        clearInvocations(mBackPressManager);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        assertNull(mCoordinator.getPanelContainerForTesting());
        verify(mSearchUiCoordinator).destroy();
        verify(mBackPressManager).removeHandler(mCoordinator);
        assertFalse(mSuggestionsListNonEmptySupplier.hasObservers());
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
                .beginQuery(
                        eq(IntentOrigin.HUB), eq(SearchType.TEXT), eq(null), eq(mWindowAndroid));
    }

    @Test
    public void testSearchUiElementsInitialized() {
        verify(mSearchUiCoordinator)
                .setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);
        verify(mUrlBarCoordinator)
                .setUrlBarHintText(
                        mActivity.getResources().getString(R.string.hub_search_empty_hint));
    }

    @Test
    public void testClickScrim_hidesOverlay() {
        showOverlay();
        mScrim.performClick();
        assertOverlayHidden();
    }

    @Test
    public void testClickCloseButton_hidesOverlay() {
        showOverlay();
        View closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);
        closeButton.performClick();
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
    public void testBringTabGroupToFront_AlreadyLocal() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        Token groupId = new Token(1, 2);
        SavedTabGroup syncGroup = new SavedTabGroup();
        syncGroup.syncId = "sync_id_1";
        syncGroup.localId = new LocalTabGroupId(groupId);

        when(mTabGroupSyncService.getGroup("group_id_1")).thenReturn(syncGroup);
        when(mTabModel.getGroupLastShownTabId(groupId)).thenReturn(42);
        when(mTabModel.getTabById(42)).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(2);

        Callback<String> callback = mBringTabGroupToFrontCallbackCaptor.getValue();
        assertNotNull(callback);

        callback.onResult("group_id_1");
        assertFalse(mCoordinator.isVisible());

        verify(mTabGroupUiActionHandler, never()).openTabGroup(any());
        verify(mTabModel).setIndex(2, TabSelectionType.FROM_USER);
    }

    @Test
    public void testBringTabGroupToFront_NotLocalOpenFirst() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        Token groupId = new Token(1, 2);
        SavedTabGroup syncGroupBefore = new SavedTabGroup();
        syncGroupBefore.syncId = "sync_id_1";
        syncGroupBefore.localId = null;

        SavedTabGroup syncGroupAfter = new SavedTabGroup();
        syncGroupAfter.syncId = "sync_id_1";
        syncGroupAfter.localId = new LocalTabGroupId(groupId);

        when(mTabGroupSyncService.getGroup("group_id_1"))
                .thenReturn(syncGroupBefore)
                .thenReturn(syncGroupAfter);
        when(mTabModel.getGroupLastShownTabId(groupId)).thenReturn(42);
        when(mTabModel.getTabById(42)).thenReturn(mTab);
        when(mTabModel.indexOf(mTab)).thenReturn(2);

        Callback<String> callback = mBringTabGroupToFrontCallbackCaptor.getValue();
        assertNotNull(callback);

        callback.onResult("group_id_1");
        assertFalse(mCoordinator.isVisible());

        verify(mTabGroupUiActionHandler).openTabGroup("sync_id_1");
        verify(mTabModel).setIndex(2, TabSelectionType.FROM_USER);
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
        // Idle the main looper to ensure all hide transition frame updates and animator listener
        // callbacks (which toggle visibility to GONE) execute completely before verification.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
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

    @Test
    public void testProfileChanged_updatesColorScheme() {
        showOverlay();
        verifySearchUiCoordinatorInitialized();

        // Switch to an incognito profile.
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        mProfileSupplier.set(mIncognitoProfile);

        // Verify that setColorScheme was called with true.
        verify(mSearchUiCoordinator).setColorScheme(true);

        // Switch back to non-incognito profile.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mProfileSupplier.set(mProfile);

        // Verify that setColorScheme was called with false.
        verify(mSearchUiCoordinator).setColorScheme(false);
    }

    @Test
    public void testBackPressSupplierState() {
        assertFalse(mCoordinator.getHandleBackPressChangedSupplier().get());

        showOverlay();
        assertTrue(mCoordinator.getHandleBackPressChangedSupplier().get());

        mCoordinator.hide();
        assertFalse(mCoordinator.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void testHandleBackPress_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        int result = mCoordinator.handleBackPress();
        assertEquals(BackPressResult.SUCCESS, result);
        assertOverlayHidden();
    }

    @Test
    public void testSuggestionsChanged_togglesEmptyState() {
        showOverlay();

        // Case 1: Search query is empty, suggestions is false.
        // Empty state should NOT be visible.
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("");
        mSuggestionsListNonEmptySupplier.set(false);
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));

        // Case 2: Search query is not empty, suggestions is true.
        // Empty state should NOT be visible.
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("abc");
        mSuggestionsListNonEmptySupplier.set(true);
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));

        // Case 3: Search query is not empty, suggestions is false.
        // Empty state SHOULD be visible.
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("abc");
        mSuggestionsListNonEmptySupplier.set(false);
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));
    }

    @Test
    public void testScrimScrollForwardedToCompositorViewHolder() {
        showOverlay();
        MotionEvent scrollEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_SCROLL, 100f, 150f, 0);
        mScrim.dispatchGenericMotionEvent(scrollEvent);

        ArgumentCaptor<MotionEvent> eventCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mCompositorViewHolder).dispatchGenericMotionEvent(eventCaptor.capture());

        MotionEvent forwardedEvent = eventCaptor.getValue();
        assertEquals(100f, forwardedEvent.getX(), 0.01f);
        assertEquals(150f, forwardedEvent.getY(), 0.01f);
        scrollEvent.recycle();
    }

    @Test
    public void testScrimScrollForwarding_RecursionGuard() {
        showOverlay();

        // Configure mock CompositorViewHolder to dispatch back to the scrim
        // when receiving the event. This simulates the ViewGroup hierarchy
        // traversing and trying to dispatch the transformed event to its child (scrim).
        doAnswer(
                        invocation -> {
                            MotionEvent event = invocation.getArgument(0);
                            return mScrim.dispatchGenericMotionEvent(event);
                        })
                .when(mCompositorViewHolder)
                .dispatchGenericMotionEvent(any(MotionEvent.class));

        MotionEvent scrollEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_SCROLL, 100f, 150f, 0);

        // This call should terminate successfully (no StackOverflowError)
        // and return false since the recursion was blocked and the target view (mock)
        // returned false to the recursive dispatch.
        assertFalse(mScrim.dispatchGenericMotionEvent(scrollEvent));

        scrollEvent.recycle();
    }

    @Test
    public void testScrimTouchForwardedToCompositorViewHolder_Drag() {
        showOverlay();

        // 1. Send ACTION_DOWN (touch start) - should be deferred and NOT forwarded yet.
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(downEvent);

        verify(mCompositorViewHolder, never()).dispatchTouchEvent(any(MotionEvent.class));

        // 2. Send ACTION_MOVE beyond touch slop (e.g., dx = 100f) to trigger drag.
        // This should trigger forwarding both the saved ACTION_DOWN and current ACTION_MOVE.
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 200f, 150f, 0);
        mScrim.dispatchTouchEvent(moveEvent);

        ArgumentCaptor<MotionEvent> touchCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mCompositorViewHolder, times(2)).dispatchTouchEvent(touchCaptor.capture());
        List<MotionEvent> forwardedEvents = touchCaptor.getAllValues();
        assertEquals(MotionEvent.ACTION_DOWN, forwardedEvents.get(0).getActionMasked());
        assertEquals(100f, forwardedEvents.get(0).getX(), 0.01f);
        assertEquals(150f, forwardedEvents.get(0).getY(), 0.01f);
        assertEquals(MotionEvent.ACTION_MOVE, forwardedEvents.get(1).getActionMasked());
        assertEquals(200f, forwardedEvents.get(1).getX(), 0.01f);
        assertEquals(150f, forwardedEvents.get(1).getY(), 0.01f);
        clearInvocations(mCompositorViewHolder);

        // 3. Send ACTION_UP (drag end) - should be forwarded directly.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 200f, 150f, 0);
        mScrim.dispatchTouchEvent(upEvent);

        verify(mCompositorViewHolder).dispatchTouchEvent(touchCaptor.capture());
        assertEquals(MotionEvent.ACTION_UP, touchCaptor.getValue().getActionMasked());

        downEvent.recycle();
        moveEvent.recycle();
        upEvent.recycle();
    }

    @Test
    public void testScrimTouchForwardedToCompositorViewHolder_Tap() {
        showOverlay();

        // Register a click listener on the scrim/model to check for dismissal
        class ClickTracker {
            boolean mClicked;
        }
        ClickTracker tracker = new ClickTracker();
        mCoordinator
                .getModelForTesting()
                .set(TabSearchOverlayProperties.ON_SCRIM_CLICK, v -> tracker.mClicked = true);

        // 1. Send ACTION_DOWN - should be deferred and NOT forwarded yet.
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(downEvent);

        verify(mCompositorViewHolder, never()).dispatchTouchEvent(any(MotionEvent.class));

        // 2. Send ACTION_UP without exceeding touch slop (same coordinate)
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(upEvent);

        // Verify that NO events (neither DOWN, UP, nor CANCEL) were sent to the compositor view,
        // since the touch sequence was a simple tap.
        verify(mCompositorViewHolder, never()).dispatchTouchEvent(any(MotionEvent.class));

        // Verify that the scrim click action was triggered to dismiss overlay
        assertTrue(tracker.mClicked);

        downEvent.recycle();
        upEvent.recycle();
    }

    @Test
    public void testScrimTouchForwardedToCompositorViewHolder_CancelDuringDrag() {
        showOverlay();

        // 1. Send ACTION_DOWN (deferred)
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(downEvent);

        // 2. Send ACTION_MOVE beyond slop to start dragging
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 200f, 150f, 0);
        mScrim.dispatchTouchEvent(moveEvent);
        clearInvocations(mCompositorViewHolder);

        // 3. Send ACTION_CANCEL - should be forwarded directly since we are dragging.
        MotionEvent cancelEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 200f, 150f, 0);
        mScrim.dispatchTouchEvent(cancelEvent);

        ArgumentCaptor<MotionEvent> touchCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mCompositorViewHolder).dispatchTouchEvent(touchCaptor.capture());
        assertEquals(MotionEvent.ACTION_CANCEL, touchCaptor.getValue().getActionMasked());

        downEvent.recycle();
        moveEvent.recycle();
        cancelEvent.recycle();
    }

    @Test
    public void testScrimTouchForwardedToCompositorViewHolder_CancelBeforeDrag() {
        showOverlay();

        // 1. Send ACTION_DOWN (deferred)
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(downEvent);

        // 2. Send ACTION_CANCEL - should NOT be forwarded because drag never started.
        MotionEvent cancelEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 100f, 150f, 0);
        mScrim.dispatchTouchEvent(cancelEvent);

        verify(mCompositorViewHolder, never()).dispatchTouchEvent(any(MotionEvent.class));

        downEvent.recycle();
        cancelEvent.recycle();
    }
}
