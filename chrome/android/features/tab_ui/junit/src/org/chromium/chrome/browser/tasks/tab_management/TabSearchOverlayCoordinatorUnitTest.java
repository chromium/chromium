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
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

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
import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxControls;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchDismissalReason;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchEntryPoint;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
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
public class TabSearchOverlayCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private TabSearchOverlayCoordinator mCoordinator;
    private View mPanelContainer;
    private View mScrim;

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SearchUiCoordinator mSearchUiCoordinator;
    @Mock private LocationBarCoordinator mLocationBarCoordinator;
    @Mock private UrlBarCoordinator mUrlBarCoordinator;
    @Mock private View mLocationBarContainerView;
    @Mock private UrlBar mUrlBar;

    @Mock(extraInterfaces = {View.OnKeyListener.class})
    private OmniboxStub mOmniboxStub;

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
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private FuseboxControls mFuseboxControls;
    @Mock private AutocompleteCoordinator mAutocompleteCoordinator;

    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();

    private final SettableNonNullObservableSupplier<Boolean> mSuggestionsListNonEmptySupplier =
            ObservableSuppliers.createNonNull(false);

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createMonotonic();
    private final TabObscuringHandler mTabObscuringHandler = new TabObscuringHandler();

    @Captor private ArgumentCaptor<OverrideUrlLoadingDelegate> mOverrideUrlLoadingDelegateCaptor;
    @Captor private ArgumentCaptor<Callback<String>> mBringTabGroupToFrontCallbackCaptor;

    @Before
    @SuppressWarnings("unchecked")
    public void setUp() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        mActivity = controller.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mTabModelSelectorSupplier.set(mTabModelSelector);
        mProfileSupplier.set(mProfile);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);

        when(mSearchUiCoordinator.getLocationBarCoordinator()).thenReturn(mLocationBarCoordinator);
        when(mLocationBarCoordinator.getUrlBarCoordinator()).thenReturn(mUrlBarCoordinator);
        when(mLocationBarCoordinator.getOmniboxStub()).thenReturn(mOmniboxStub);
        when(mLocationBarCoordinator.getOmniboxSuggestionsVisualState())
                .thenReturn(mAutocompleteCoordinator);
        when(mLocationBarCoordinator.getContainerView()).thenReturn(mLocationBarContainerView);
        when(mLocationBarContainerView.findViewById(R.id.url_bar)).thenReturn(mUrlBar);
        when(mOmniboxStub.isUrlBarFocused()).thenReturn(true);
        when(mLocationBarCoordinator.getSuggestionsListNonEmptySupplier())
                .thenReturn(mSuggestionsListNonEmptySupplier);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);

        mCoordinator =
                new TabSearchOverlayCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mProfileSupplier,
                        mSnackbarManager,
                        ObservableSuppliers.createNonNull(mModalDialogManager),
                        mActivityLifecycleDispatcher,
                        mTabModelSelectorSupplier,
                        /* edgeToEdgeSystemBarColorHelper= */ null,
                        mBackPressManager,
                        ObservableSuppliers.createNonNull(mCompositorViewHolder),
                        mTabGroupUiActionHandlerSupplier,
                        mDesktopWindowStateManager,
                        mTabObscuringHandler,
                        mFuseboxControls);
        mCoordinator.setSearchUiCoordinatorForTesting(mSearchUiCoordinator);

        // Inflate the overlay and initialize member views.
        mCoordinator.ensureInitialized();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mPanelContainer = mCoordinator.getPanelContainerForTesting();
        assertNotNull(mPanelContainer);
        mScrim = mPanelContainer.findViewById(R.id.tab_search_overlay_scrim);

        assertTrue(mSuggestionsListNonEmptySupplier.hasObservers());

        verify(mBackPressManager)
                .addHandler(mCoordinator, BackPressHandler.Type.TAB_SEARCH_OVERLAY);
        verify(mActivityLifecycleDispatcher).register(mCoordinator);

        // Clear mock invocations from setup phase to ensure test assertions are isolated.
        clearInvocations(mLocationBarCoordinator);
        clearInvocations(mBackPressManager);
        clearInvocations(mActivityLifecycleDispatcher);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        assertNull(mCoordinator.getPanelContainerForTesting());
        assertNull(mCoordinator.getPopupWindowForTesting());
        verify(mSearchUiCoordinator).destroy();
        verify(mBackPressManager).removeHandler(mCoordinator);
        verify(mActivityLifecycleDispatcher).unregister(mCoordinator);
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
    public void testShow_endsFuseboxInput() {
        showOverlay();
        verify(mFuseboxControls).endFuseboxInput();
    }

    @Test
    public void testShow_nullFuseboxControls_doesNotThrow() {
        mCoordinator.destroy();
        clearInvocations(mSearchUiCoordinator);
        mCoordinator =
                new TabSearchOverlayCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mProfileSupplier,
                        mSnackbarManager,
                        ObservableSuppliers.createNonNull(mModalDialogManager),
                        mActivityLifecycleDispatcher,
                        mTabModelSelectorSupplier,
                        /* edgeToEdgeSystemBarColorHelper= */ null,
                        mBackPressManager,
                        ObservableSuppliers.createNonNull(mCompositorViewHolder),
                        mTabGroupUiActionHandlerSupplier,
                        mDesktopWindowStateManager,
                        mTabObscuringHandler,
                        /* fuseboxControls= */ null);
        mCoordinator.setSearchUiCoordinatorForTesting(mSearchUiCoordinator);
        mCoordinator.ensureInitialized();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mPanelContainer = mCoordinator.getPanelContainerForTesting();

        showOverlay();
        assertTrue(mCoordinator.isVisible());
    }

    @Test
    public void testShow_alreadyVisible_doesNotInvokeEndFuseboxInput() {
        showOverlay();
        verify(mFuseboxControls, times(1)).endFuseboxInput();

        // Calling show again while visible should not re-invoke endFuseboxInput.
        mCoordinator.show(TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        verify(mFuseboxControls, times(1)).endFuseboxInput();
    }

    @Test
    public void testShow_recordsEntryPointHistogram() {
        var watcherHorizontal =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        mCoordinator.show(TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        watcherHorizontal.assertExpected();
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);

        var watcherVertical =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.VERTICAL_TABS);
        mCoordinator.show(TabSearchEntryPoint.VERTICAL_TABS);
        watcherVertical.assertExpected();
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);

        var watcherShortcut =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.KEYBOARD_SHORTCUT);
        mCoordinator.show(TabSearchEntryPoint.KEYBOARD_SHORTCUT);
        watcherShortcut.assertExpected();
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
    }

    @Test
    public void testSearchUiElementsInitialized() {
        verify(mSearchUiCoordinator)
                .setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);
        verify(mUrlBarCoordinator)
                .setUrlBarHintText(
                        mActivity.getResources().getString(R.string.hub_search_empty_hint));
        verify(mUrlBar).setTextAppearance(R.style.TextAppearance_TextMedium);
    }

    @Test
    public void testEmptyStateViewSetup() {
        ImageView emptyStateIcon = mPanelContainer.findViewById(R.id.empty_state_icon);
        TextView emptyStateTitle = mPanelContainer.findViewById(R.id.empty_state_text_title);
        TextView emptyStateDescription =
                mPanelContainer.findViewById(R.id.empty_state_text_description);

        assertNotNull(emptyStateIcon);
        assertNotNull(emptyStateTitle);
        assertNotNull(emptyStateDescription);

        Drawable drawable = emptyStateIcon.getDrawable();
        assertNotNull(drawable);
        assertEquals(
                R.drawable.tab_search_empty_state,
                Shadows.shadowOf(drawable).getCreatedFromResId());
        assertEquals(
                mActivity.getResources().getString(R.string.search_in_settings_no_match),
                emptyStateTitle.getText().toString());
        assertEquals(View.GONE, emptyStateDescription.getVisibility());

        float expectedSize = mActivity.getResources().getDimension(R.dimen.text_size_large);
        assertEquals(expectedSize, emptyStateTitle.getTextSize(), 0.01f);
    }

    @Test
    public void testClickScrim_hidesOverlay() {
        showOverlay();
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.SCRIM);
        mScrim.performClick();
        watcher.assertExpected();
        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
        assertOverlayHidden();
    }

    @Test
    public void testClickCloseButton_hidesOverlay() {
        showOverlay();
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.CLOSE_BUTTON);
        View closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);
        closeButton.performClick();
        watcher.assertExpected();
        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
        assertOverlayHidden();
    }

    @Test
    public void testHide_hidesOverlayAndClearsFocus() {
        showOverlay();
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.CLOSE_BUTTON);
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        watcher.assertExpected();
        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
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
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.URL_LOADED);
        boolean handled = delegate.willHandleLoadUrlWithPostData(params, /* incognito= */ false);
        watcher.assertExpected();
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
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.URL_LOADED);
        boolean handled = delegate.willHandleLoadUrlWithPostData(params, /* incognito= */ true);
        watcher.assertExpected();
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

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.TAB_GROUP_SELECTED);
        callback.onResult("group_id_1");
        watcher.assertExpected();
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

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.TAB_GROUP_SELECTED);
        callback.onResult("group_id_1");
        watcher.assertExpected();
        assertFalse(mCoordinator.isVisible());

        verify(mTabGroupUiActionHandler).openTabGroup("sync_id_1");
        verify(mTabModel).setIndex(2, TabSelectionType.FROM_USER);
    }

    private void showOverlay() {
        mCoordinator.show(TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        assertOverlayShown();
    }

    private void assertOverlayShown() {
        assertTrue(mCoordinator.isVisible());
        assertEquals(View.VISIBLE, mPanelContainer.getVisibility());
        assertNotNull(mCoordinator.getPopupWindowForTesting());
        assertTrue(mCoordinator.getPopupWindowForTesting().isShowing());
    }

    private void assertOverlayHidden() {
        // Idle the main looper to ensure all hide transition frame updates and animator listener
        // callbacks (which toggle visibility to GONE) execute completely before verification.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);
        assertFalse(mCoordinator.isVisible());
        assertEquals(View.GONE, mPanelContainer.getVisibility());
        assertNotNull(mCoordinator.getPopupWindowForTesting());
        assertFalse(mCoordinator.getPopupWindowForTesting().isShowing());
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

        ImageButton closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);

        // Switch to an incognito profile.
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        mProfileSupplier.set(mIncognitoProfile);

        // Verify that setColorScheme was called with true.
        verify(mSearchUiCoordinator).setColorScheme(true);

        // Verify close button colors in incognito.
        assertEquals(
                AppCompatResources.getColorStateList(mActivity, R.color.default_icon_color_light),
                closeButton.getImageTintList());
        assertEquals(
                AppCompatResources.getColorStateList(
                        mActivity, R.color.tab_strip_close_bg_incognito_tint_list),
                closeButton.getBackgroundTintList());

        // Switch back to non-incognito profile.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mProfileSupplier.set(mProfile);

        // Verify that setColorScheme was called with false.
        verify(mSearchUiCoordinator).setColorScheme(false);

        // Verify close button colors in standard.
        assertEquals(
                AppCompatResources.getColorStateList(
                        mActivity, R.color.default_icon_color_tint_list),
                closeButton.getImageTintList());
        assertEquals(
                AppCompatResources.getColorStateList(
                        mActivity, R.color.tab_strip_close_bg_tint_list),
                closeButton.getBackgroundTintList());
    }

    @Test
    public void testBackPressSupplierState() {
        assertFalse(mCoordinator.getHandleBackPressChangedSupplier().get());

        showOverlay();
        assertTrue(mCoordinator.getHandleBackPressChangedSupplier().get());

        mCoordinator.hide(TabSearchDismissalReason.BACK_PRESS);
        assertFalse(mCoordinator.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void testHandleBackPress_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.BACK_PRESS);
        int result = mCoordinator.handleBackPress();
        watcher.assertExpected();
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
    public void testSessionHadEmptyState_recordsHistogram() {
        // Case 1: Session without empty state records false on hide.
        showOverlay();
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("abc");
        mSuggestionsListNonEmptySupplier.set(true);
        var watcherNoEmpty =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.SessionHadEmptyState", false);
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        watcherNoEmpty.assertExpected();

        // Case 2: Session with empty state records true on hide.
        showOverlay();
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("xyz");
        mSuggestionsListNonEmptySupplier.set(false);
        assertTrue(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));
        var watcherWithEmpty =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.SessionHadEmptyState", true);
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        watcherWithEmpty.assertExpected();

        // Case 3: Subsequent session resets flag and records false if no empty state occurs.
        showOverlay();
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("abc");
        mSuggestionsListNonEmptySupplier.set(true);
        var watcherReset =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.SessionHadEmptyState", false);
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        watcherReset.assertExpected();
    }

    @Test
    public void testScrimClick_dismissalSequence() {
        showOverlay();
        // Setup state: search query is not empty, suggestions list is not empty (empty state not
        // visible).
        when(mUrlBarCoordinator.getTextWithoutAutocomplete()).thenReturn("abc");
        mSuggestionsListNonEmptySupplier.set(true);
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));

        // 1. Click scrim to dismiss.
        mScrim.performClick();

        // 2. During the hide animation (before idling looper):
        // - Overlay visibility property is set to false.
        assertFalse(mCoordinator.isVisible());
        // - Suggestions list is STILL non-empty.
        assertTrue(mSuggestionsListNonEmptySupplier.get());
        // - Empty state is NOT visible.
        assertFalse(
                mCoordinator
                        .getModelForTesting()
                        .get(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE));

        // 3. Complete the animation by idling the looper.
        ShadowLooper.idleMainLooper(1, TimeUnit.SECONDS);

        // 4. After the animation completes:
        // - Focus is cleared.
        verify(mLocationBarCoordinator).clearOmniboxFocus();
        // - Simulate the location bar updating its focus status.
        when(mOmniboxStub.isUrlBarFocused()).thenReturn(false);
        // - Simulate suggestions list becoming empty due to focus loss.
        mSuggestionsListNonEmptySupplier.set(false);

        // - Empty state remains NOT visible.
        assertFalse(
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

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testSystemGestureExclusionRects_ShowAndHide() {
        showOverlay();
        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        View closeButton = panelView.findViewById(R.id.tab_search_close_button);

        // Mock layout bounds for close button.
        closeButton.setLeft(228);
        closeButton.setTop(4);
        closeButton.setRight(260);
        closeButton.setBottom(36);

        // Trigger layout pass on panelView.
        panelView.layout(0, 0, 264, 500);

        // Verify exclusion rect matches close button bounds.
        List<Rect> exclusionRects = mPanelContainer.getSystemGestureExclusionRects();
        assertEquals(1, exclusionRects.size());
        assertEquals(new Rect(228, 4, 260, 36), exclusionRects.get(0));

        // Hide overlay and verify exclusion rect is cleared.
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        assertTrue(mPanelContainer.getSystemGestureExclusionRects().isEmpty());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testSystemGestureExclusionRects_DesktopWindowing() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(40);

        showOverlay();

        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        View closeButton = panelView.findViewById(R.id.tab_search_close_button);
        closeButton.setLeft(228);
        closeButton.setTop(4);
        closeButton.setRight(260);
        closeButton.setBottom(36);

        // Perform layout on panelView and mPanelContainer.
        panelView.layout(0, 0, 264, 500);
        mPanelContainer.layout(0, 0, 800, 500);

        // Verify exclusion rects contain:
        // 1. Close button rect: (228, 4, 260, 36)
        // 2. Full header rect: (0, 0, 800, 40)
        List<Rect> exclusionRects = mPanelContainer.getSystemGestureExclusionRects();
        assertEquals(2, exclusionRects.size());
        assertEquals(new Rect(228, 4, 260, 36), exclusionRects.get(0));
        assertEquals(new Rect(0, 0, 800, 40), exclusionRects.get(1));

        // Hide overlay and verify exclusion rects are cleared.
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        assertTrue(mPanelContainer.getSystemGestureExclusionRects().isEmpty());
    }

    @Test
    public void testPanelTopMargin_AlignsWithControlContainer() {
        int[] containerLocation = new int[] {0, 48};
        View controlContainer =
                new View(mActivity) {
                    @Override
                    public void getLocationInWindow(int[] outLocation) {
                        outLocation[0] = containerLocation[0];
                        outLocation[1] = containerLocation[1];
                    }
                };
        controlContainer.setId(R.id.control_container);
        mActivity.setContentView(controlContainer);

        showOverlay();

        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        var params = (LinearLayout.LayoutParams) panelView.getLayoutParams();
        assertEquals(48, params.topMargin);

        // In desktop windowing mode, control container sits at y = 0.
        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        containerLocation[1] = 0;
        showOverlay();

        params = (LinearLayout.LayoutParams) panelView.getLayoutParams();
        assertEquals(0, params.topMargin);
    }

    @Test
    public void testScrimNonScrollGenericMotionEvent_ConsumedAndNotForwarded() {
        showOverlay();
        MotionEvent clickEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_BUTTON_PRESS, 100f, 150f, 0);
        assertTrue(mScrim.dispatchGenericMotionEvent(clickEvent));

        MotionEvent hoverEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 100f, 150f, 0);
        assertTrue(mScrim.dispatchGenericMotionEvent(hoverEvent));

        verify(mCompositorViewHolder, never()).dispatchGenericMotionEvent(any(MotionEvent.class));
        clickEvent.recycle();
        hoverEvent.recycle();
    }

    @Test
    public void testWindowFocusLost_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.WINDOW_FOCUS_LOST);
        mCoordinator.getWindowFocusListenerForTesting().onWindowFocusChanged(false);
        watcher.assertExpected();

        assertOverlayHidden();
    }

    @Test
    public void testWindowFocusLostWhenHidden_doesNothing() {
        assertFalse(mCoordinator.isVisible());

        mCoordinator.getWindowFocusListenerForTesting().onWindowFocusChanged(false);

        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
    }

    @Test
    public void testCloseButtonSizing_NonDesktopDensity() {
        ImageButton closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);

        int expectedSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_search_close_button_size);
        assertEquals(expectedSize, closeButton.getLayoutParams().width);
        assertEquals(expectedSize, closeButton.getLayoutParams().height);
    }

    @Test
    public void testCloseButtonSizing_DesktopDensity() {
        DeviceInfo.setIsDesktopForTesting(true);

        // Re-create the coordinator under desktop density condition
        mCoordinator.destroy();
        clearInvocations(mSearchUiCoordinator);
        mCoordinator =
                new TabSearchOverlayCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mProfileSupplier,
                        mSnackbarManager,
                        ObservableSuppliers.createNonNull(mModalDialogManager),
                        mActivityLifecycleDispatcher,
                        mTabModelSelectorSupplier,
                        /* edgeToEdgeSystemBarColorHelper= */ null,
                        mBackPressManager,
                        ObservableSuppliers.createNonNull(mCompositorViewHolder),
                        mTabGroupUiActionHandlerSupplier,
                        mDesktopWindowStateManager,
                        mTabObscuringHandler,
                        mFuseboxControls);
        mCoordinator.setSearchUiCoordinatorForTesting(mSearchUiCoordinator);
        mCoordinator.ensureInitialized();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        View panelContainer = mCoordinator.getPanelContainerForTesting();
        assertNotNull(panelContainer);
        ImageButton closeButton = panelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);

        int expectedSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_search_close_button_size_desktop);
        assertEquals(expectedSize, closeButton.getLayoutParams().width);
        assertEquals(expectedSize, closeButton.getLayoutParams().height);
    }

    @Test
    public void testOnConfigurationChanged_whenVisible_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.WINDOW_RESIZED);
        mCoordinator.onConfigurationChanged(new Configuration());
        watcher.assertExpected();

        assertOverlayHidden();
    }

    @Test
    public void testOnConfigurationChanged_whenHidden_doesNothing() {
        assertFalse(mCoordinator.isVisible());

        mCoordinator.onConfigurationChanged(new Configuration());

        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
    }

    @Test
    public void testOnAppHeaderStateChanged_whenVisible_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.WINDOW_RESIZED);
        mCoordinator.onAppHeaderStateChanged(new AppHeaderState());
        watcher.assertExpected();

        assertOverlayHidden();
    }

    @Test
    public void testOnAppHeaderStateChanged_whenHidden_doesNothing() {
        assertFalse(mCoordinator.isVisible());

        mCoordinator.onAppHeaderStateChanged(new AppHeaderState());

        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
    }

    @Test
    public void testOnDesktopWindowingModeChanged_whenVisible_hidesOverlay() {
        showOverlay();
        assertTrue(mCoordinator.isVisible());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.WINDOW_RESIZED);
        mCoordinator.onDesktopWindowingModeChanged(true);
        watcher.assertExpected();

        assertOverlayHidden();
    }

    @Test
    public void testOnDesktopWindowingModeChanged_whenHidden_doesNothing() {
        assertFalse(mCoordinator.isVisible());

        mCoordinator.onDesktopWindowingModeChanged(true);

        verify(mLocationBarCoordinator, never()).clearOmniboxFocus();
    }

    @Test
    public void testShow_obscuresTabsAndToolbar() {
        assertFalse(mTabObscuringHandler.isToolbarObscured());
        assertFalse(mTabObscuringHandler.isTabContentObscured());

        mCoordinator.show(TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        assertTrue(mTabObscuringHandler.isToolbarObscured());
        assertTrue(mTabObscuringHandler.isTabContentObscured());
    }

    @Test
    public void testHide_unobscuresTabsAndToolbar() {
        mCoordinator.show(TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);
        assertTrue(mTabObscuringHandler.isToolbarObscured());
        assertTrue(mTabObscuringHandler.isTabContentObscured());

        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);
        assertFalse(mTabObscuringHandler.isToolbarObscured());
        assertFalse(mTabObscuringHandler.isTabContentObscured());
    }

    @Test
    public void testPopupWindow_configuration() {
        PopupWindow popupWindow = mCoordinator.getPopupWindowForTesting();
        assertNotNull(popupWindow);
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, popupWindow.getWidth());
        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, popupWindow.getHeight());
        assertTrue(popupWindow.isFocusable());
        assertTrue(popupWindow.isOutsideTouchable());
        assertFalse(popupWindow.isClippingEnabled());
        assertEquals(PopupWindow.INPUT_METHOD_NEEDED, popupWindow.getInputMethodMode());
        assertEquals(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE,
                popupWindow.getSoftInputMode());
    }

    @Test
    public void testPopupWindow_dismiss_hidesOverlayWithBackPressReason() {
        showOverlay();

        PopupWindow popupWindow = mCoordinator.getPopupWindowForTesting();
        assertNotNull(popupWindow);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.BACK_PRESS);
        popupWindow.dismiss();
        watcher.assertExpected();
        assertOverlayHidden();
    }

    @Test
    public void testAccessibility_layoutAttributes() {
        View panel = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        assertNotNull(panel);
        assertFalse(panel.isFocusable());

        View scrim = mPanelContainer.findViewById(R.id.tab_search_overlay_scrim);
        assertNotNull(scrim);
        assertFalse(scrim.isFocusable());
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, scrim.getImportantForAccessibility());

        ImageButton closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);
        assertTrue(closeButton.isFocusable());
        assertTrue(closeButton.isFocusableInTouchMode());
        assertTrue(closeButton.isClickable());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES, closeButton.getImportantForAccessibility());
        assertEquals(R.id.search_activity_container, closeButton.getAccessibilityTraversalBefore());
        verify(mUrlBar).setAccessibilityTraversalAfter(R.id.tab_search_close_button);
        assertEquals(
                mActivity.getString(R.string.close),
                closeButton.getContentDescription().toString());
    }

    @Test
    public void testAccessibility_paneTitleLifecycle() {
        View panel = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        ImageButton closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(panel);
        assertNotNull(closeButton);

        assertNull(ViewCompat.getAccessibilityPaneTitle(panel));

        showOverlay();

        assertEquals(
                mActivity.getString(R.string.keyboard_shortcut_tab_search),
                ViewCompat.getAccessibilityPaneTitle(panel));

        mCoordinator.hide(TabSearchDismissalReason.CLOSE_BUTTON);

        assertNull(ViewCompat.getAccessibilityPaneTitle(panel));
    }

    @Test
    public void testKeyNavigation_betweenUrlBarAndCloseButton() {
        ImageButton closeButton = mPanelContainer.findViewById(R.id.tab_search_close_button);
        assertNotNull(closeButton);

        ArgumentCaptor<View.OnKeyListener> urlBarKeyListenerCaptor =
                ArgumentCaptor.forClass(View.OnKeyListener.class);
        verify(mUrlBar).setKeyDownListener(urlBarKeyListenerCaptor.capture());
        View.OnKeyListener urlBarKeyListener = urlBarKeyListenerCaptor.getValue();
        assertNotNull(urlBarKeyListener);

        KeyEvent shiftTabEvent =
                new KeyEvent(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_TAB,
                        /* repeat= */ 0,
                        KeyEvent.META_SHIFT_ON);
        KeyEvent upEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP);

        // When at the top suggestion (selectedIndex == 0), Shift+Tab and Up on UrlBar
        // unselect the suggestion (returning focus to UrlBar) and do not focus Close button.
        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(0);
        assertTrue(urlBarKeyListener.onKey(mUrlBar, KeyEvent.KEYCODE_TAB, shiftTabEvent));
        verify(mAutocompleteCoordinator).resetSelection();
        assertFalse(closeButton.isFocused());

        // When unselected in UrlBar (selectedIndex == null), Shift+Tab and Up focus Close button.
        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(null);
        assertTrue(urlBarKeyListener.onKey(mUrlBar, KeyEvent.KEYCODE_TAB, shiftTabEvent));
        assertTrue(closeButton.isFocused());

        assertTrue(urlBarKeyListener.onKey(mUrlBar, KeyEvent.KEYCODE_DPAD_UP, upEvent));

        // When deeper in suggestions (selectedIndex == 1), Up delegates to omnibox stub.
        when(mAutocompleteCoordinator.getSelectedIndex()).thenReturn(1);
        View.OnKeyListener omniboxListener = (View.OnKeyListener) mOmniboxStub;
        when(omniboxListener.onKey(eq(mUrlBar), eq(KeyEvent.KEYCODE_DPAD_UP), eq(upEvent)))
                .thenReturn(true);
        assertTrue(urlBarKeyListener.onKey(mUrlBar, KeyEvent.KEYCODE_DPAD_UP, upEvent));

        // Forward Tab on Close button returns focus to UrlBar.
        KeyEvent tabEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);
        closeButton.dispatchKeyEvent(tabEvent);
        verify(mUrlBar).requestFocus();

        // Down arrow on Close button returns focus to UrlBar.
        KeyEvent downEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN);
        closeButton.dispatchKeyEvent(downEvent);
        verify(mUrlBar, times(2)).requestFocus();
    }
}
