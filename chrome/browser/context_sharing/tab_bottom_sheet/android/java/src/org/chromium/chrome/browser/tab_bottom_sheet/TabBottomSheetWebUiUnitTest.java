// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentCaptor.captor;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

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
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;

/** Unit tests for {@link TabBottomSheetWebUi}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetWebUiUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WebContents mWebContents;
    @Mock private ThinWebView mThinWebView;
    @Mock private ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    @Mock private SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
    @Mock private Window mMockWindow;
    @Mock private EventForwarder mEventForwarder;
    @Mock private CoBrowseComponentProvider mMockComponentProvider;
    @Mock private ResizingPlaceholderCoordinator mMockPlaceholderCoordinator;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private ContentView mContentView;
    private TabBottomSheetWebUi mWebUi;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivity = mActivityController.get();
        // A null WebContents keeps ContentView from calling into native-backed WebContents
        // helpers (e.g. ViewEventSink) on focus / attach changes.
        mContentView = ContentView.createContentView(mActivity, /* webContents= */ null);

        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
        when(mThinWebView.getView()).thenReturn(new View(mActivity));

        WeakReference<Activity> weakActivity = new WeakReference<>(mActivity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);

        View decorView = new View(mActivity);
        decorView.layout(0, 0, /* right= */ 0, /* bottom= */ 1000);
        when(mWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindow.getDecorView()).thenReturn(decorView);

        Mockito.lenient().doReturn(mEventForwarder).when(mWebContents).getEventForwarder();
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        mWebUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ null,
                        mContentView);
        TabBottomSheetWebUi.setInTestModeForTesting();
    }

    @Test
    public void testConstructor_WithComponentProvider() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView = new View(context);
        TabBottomSheetWebUi webUi =
                new TabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        null,
                        null,
                        mMockComponentProvider);
        assertEquals(
                mMockComponentProvider,
                webUi.getWebViewResizingHelper().getComponentProviderForTesting());
    }

    @Test
    public void testConstructor_WithNullComponentProvider() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView = new View(context);
        TabBottomSheetWebUi webUi =
                new TabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        null,
                        null,
                        null);
        assertNull(webUi.getWebViewResizingHelper().getComponentProviderForTesting());
    }

    @Test
    public void testSetWebContents_SameWebContents_Noop() {
        mWebUi.setWebContents(mWebContents, true);
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());

        mWebUi.setWebContents(mWebContents, true);
        // Verify it was not called again.
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());
    }

    @Test
    public void testSetWebContents_DifferentWebContents_Updates() {
        mWebUi.setWebContents(mWebContents, true);
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());

        WebContents secondWebContents = mock(WebContents.class);
        Mockito.doReturn(mEventForwarder).when(secondWebContents).getEventForwarder();

        mWebUi.setWebContents(secondWebContents, true);
        verify(secondWebContents, times(1))
                .setDelegates(any(), any(), any(), eq(mWindowAndroid), any());
    }

    @Test
    public void testSetWebContents_UpdatesDelegatesWhenAlreadySet() {
        ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(null);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewDelegate);

        mWebUi.setWebContents(mWebContents, true);

        verify(mWebContents, times(0)).setDelegates(any(), any(), any(), any(), any());
        verify(mWebContents, times(1)).setTopLevelNativeWindow(eq(mWindowAndroid));
        assertNotNull(viewDelegate.getContainerView());
    }

    @Test
    public void testFocusHandling() {
        mWebUi.setWebContents(mWebContents, true);
        // Attaching registers the window focus listener.
        attachToActivityWithFocusableSibling(mContentView);
        assertTrue(mContentView.requestFocus());

        mActivityController.windowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        assertFalse(mContentView.isFocused());

        assertTrue(mContentView.requestFocus());
        mActivityController.windowFocusChanged(true);
        ShadowLooper.idleMainLooper();
        assertTrue(mContentView.isFocused());

        // Detaching unregisters the listener: once moved to another window, losing focus in the
        // original window no longer clears the ContentView's focus.
        ((ViewGroup) mContentView.getParent()).removeView(mContentView);
        Activity otherActivity = Robolectric.buildActivity(Activity.class).setup().get();
        otherActivity.setContentView(mContentView);
        assertTrue(mContentView.requestFocus());
        mActivityController.windowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        assertTrue(mContentView.isFocused());
    }

    @Test
    public void testSetWebContents_Null_ResetsThinWebView() {
        WebContents nonNullWebContents = mock(WebContents.class);
        EventForwarder mockEventForwarder = mock(EventForwarder.class);
        Mockito.doReturn(mockEventForwarder).when(nonNullWebContents).getEventForwarder();

        mWebUi.setWebContents(nonNullWebContents, true);
        mWebUi.setWebContents(null, false);
        verify(mThinWebView, times(1)).destroy();
    }

    @Test
    public void testSetWebContents_Null_ActivityDestroyed_DoesNotRecreateThinWebView() {
        WebContents nonNullWebContents = mock(WebContents.class);
        EventForwarder mockEventForwarder = mock(EventForwarder.class);
        Mockito.doReturn(mockEventForwarder).when(nonNullWebContents).getEventForwarder();

        mWebUi.setWebContents(nonNullWebContents, true);

        ActivityController<Activity> destroyedController =
                Robolectric.buildActivity(Activity.class).setup();
        Activity destroyedActivity = destroyedController.get();
        destroyedController.destroy();
        assertTrue(destroyedActivity.isDestroyed());
        WeakReference<Activity> weakActivity = new WeakReference<>(destroyedActivity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);

        // Reset verification state of mThinWebView
        Mockito.reset(mThinWebView);

        mWebUi.setWebContents(null, false);

        // Verify that mThinWebView's destroy was called (the first one is destroyed).
        verify(mThinWebView, times(1)).destroy();
        // Verify that mThinWebView is now null in the WebUi.
        assertNull(mWebUi.getThinWebViewForTesting());
    }

    @Test
    public void testSetWebContents_Null_ActivityReferenceCleared_DoesNotRecreateThinWebView() {
        WebContents nonNullWebContents = mock(WebContents.class);
        EventForwarder mockEventForwarder = mock(EventForwarder.class);
        Mockito.doReturn(mockEventForwarder).when(nonNullWebContents).getEventForwarder();

        mWebUi.setWebContents(nonNullWebContents, true);

        WeakReference<Activity> weakActivity = new WeakReference<>(null);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);

        // Reset verification state of mThinWebView
        Mockito.reset(mThinWebView);

        mWebUi.setWebContents(null, false);

        // Verify that mThinWebView's destroy was called (the first one is destroyed).
        verify(mThinWebView, times(1)).destroy();
        // Verify that mThinWebView is now null in the WebUi.
        assertNull(mWebUi.getThinWebViewForTesting());
    }

    @Test
    public void testDestroy() {
        mWebUi.setWebContents(mWebContents, true);
        mWebUi.destroy();
        verify(mThinWebView).destroy();
        verify(mWindowAndroid).removeActivityStateObserver(any());
        assertNull(mWebUi.getWebContents());
    }

    @Test
    public void testDestroy_destroysPlaceholderCoordinator() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        when(mMockComponentProvider.createResizingPlaceholderCoordinator(
                        any(), eq(Color.WHITE), eq(Color.LTGRAY)))
                .thenReturn(mMockPlaceholderCoordinator);
        View containerView = new View(context);
        TabBottomSheetWebUi webUi =
                new TabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        null,
                        null,
                        mMockComponentProvider);

        webUi.destroy();
        verify(mMockPlaceholderCoordinator).destroy();
    }

    @Test
    public void testGetWebUiView() {
        View view = mWebUi.getWebUiView();
        assertNotNull(view);
    }

    @Test
    public void testSetWebContents_resetsTouchOffset() {
        mWebUi.setWebContents(mWebContents, true);

        verify(mEventForwarder).setCurrentTouchOffsetX(0.0f);
        verify(mEventForwarder).setCurrentTouchOffsetY(0.0f);
    }

    @Test
    public void testSetWebContents_clearsActivityFocus() {
        View focusedView = createFocusableView(mActivity);
        attachToActivityWithFocusableSibling(focusedView);
        assertTrue(focusedView.requestFocus());
        shadowOf(mActivity).setCurrentFocus(focusedView);

        mWebUi.setWebContents(mWebContents, true);

        assertFalse(focusedView.isFocused());
    }

    @Test
    public void testSetWebContents_noFocus_doesNotClearActivityFocus() {
        View focusedView = createFocusableView(mActivity);
        attachToActivityWithFocusableSibling(focusedView);
        assertTrue(focusedView.requestFocus());
        shadowOf(mActivity).setCurrentFocus(focusedView);

        mWebUi.setWebContents(mWebContents, false);

        assertTrue(focusedView.isFocused());
    }

    @Test
    public void testSetWebContents_ItemDelegate_BottomSheet() {
        mWebUi.setWebContents(mWebContents, true);
        ArgumentCaptor<ContextMenuItemDelegate> captor =
                ArgumentCaptor.forClass(ContextMenuItemDelegate.class);
        verify(mContextMenuPopulatorFactory).setItemDelegate(captor.capture());

        ContextMenuItemDelegate delegate = captor.getValue();
        assertNotNull(delegate);
        assertTrue(delegate instanceof ThinWebViewContextMenuItemDelegate);
        assertNull(
                ((ThinWebViewContextMenuItemDelegate) delegate)
                        .getIntentTargetClassNameForTesting());
        assertFalse(delegate.supportsOpenImageInNewTab());
        assertFalse(delegate.supportsOpenInEphemeralTab());
        assertFalse(delegate.supportsSaveImage());
        assertFalse(delegate.supportsSearchByImage());
        assertFalse(delegate.supportsInspectElement());
    }

    @Test
    public void testSetWebContents_ItemDelegate_SidePanel() {
        ContextMenuPopulatorFactory mockFactory = mock(ContextMenuPopulatorFactory.class);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        TabBottomSheetWebUi sidePanelWebUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mockFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.SIDE_PANEL,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ null,
                        mContentView);
        sidePanelWebUi.setWebContents(mWebContents, true);

        ArgumentCaptor<ContextMenuItemDelegate> captor =
                ArgumentCaptor.forClass(ContextMenuItemDelegate.class);
        verify(mockFactory).setItemDelegate(captor.capture());

        ContextMenuItemDelegate delegate = captor.getValue();
        assertNotNull(delegate);
        assertTrue(delegate instanceof ThinWebViewContextMenuItemDelegate);
        assertEquals(
                BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME,
                ((ThinWebViewContextMenuItemDelegate) delegate)
                        .getIntentTargetClassNameForTesting());
        assertTrue(delegate.supportsOpenImageInNewTab());
        assertTrue(delegate.supportsOpenInEphemeralTab());
        assertTrue(delegate.supportsSaveImage());
        assertTrue(delegate.supportsSearchByImage());
        assertTrue(delegate.supportsInspectElement());
    }

    @Test
    public void testSetWebContents_ItemDelegate_LinkOpener_SidePanel() {
        ContextMenuPopulatorFactory mockFactory = mock(ContextMenuPopulatorFactory.class);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        TabBottomSheetWebUi sidePanelWebUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mockFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.SIDE_PANEL,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ null,
                        mContentView);
        sidePanelWebUi.setWebContents(mWebContents, true);

        ArgumentCaptor<ContextMenuItemDelegate> delegateCaptor =
                ArgumentCaptor.forClass(ContextMenuItemDelegate.class);
        verify(mockFactory).setItemDelegate(delegateCaptor.capture());
        ContextMenuItemDelegate delegate = delegateCaptor.getValue();

        GURL url = new GURL("https://example.com");

        // Test New Tab
        delegate.onOpenInNewTab(url, null, false, null);
        Intent newTabIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(newTabIntent);
        assertEquals(Intent.ACTION_VIEW, newTabIntent.getAction());
        assertEquals(url.getSpec(), newTabIntent.getData().toString());

        // Test New Tab in Group
        TabModelSelector mockSelector = mock(TabModelSelector.class);
        TabModelSelectorSupplier.setInstanceForTesting(mockSelector);
        Tab mockTab = mock(Tab.class);
        when(mockSelector.getCurrentTab()).thenReturn(mockTab);
        when(mockTab.isIncognito()).thenReturn(false);

        delegate.onOpenInNewTabInGroup(url, null, null);
        verify(mockSelector)
                .openNewTab(
                        any(),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP),
                        eq(mockTab),
                        eq(false));
        TabModelSelectorSupplier.setInstanceForTesting(null);
        assertNull(shadowOf(mActivity).getNextStartedActivity());

        // Test Incognito Tab
        delegate.onOpenInNewIncognitoTab(url);
        Intent incognitoIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(incognitoIntent);
        assertTrue(
                incognitoIntent.getBooleanExtra(
                        BrowserIntentUtils.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));

        MultiInstanceOrchestrator mockOrchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mockOrchestrator);

        // Test New Window
        delegate.openInOtherWindow(url, null, false, true, null);
        verify(mockOrchestrator)
                .openUrlInOtherWindow(
                        eq(mActivity), any(), eq(Tab.INVALID_TAB_ID), eq(true), eq(false));

        // Test Incognito Window
        delegate.openInIncognitoWindow(url);
        verify(mockOrchestrator)
                .openUrlInOtherWindow(
                        eq(mActivity), any(), eq(Tab.INVALID_TAB_ID), eq(false), eq(true));

        MultiInstanceOrchestratorFactory.setInstanceForTesting(null);
    }

    @Test
    public void testSelectionDropdownWrapper_ignoresFocusClearWhenShowing() {
        mWebUi.setWebContents(mWebContents, true);
        attachToActivityWithFocusableSibling(mContentView);

        // Capture the wrapped delegate.
        ArgumentCaptor<ThinWebViewAttachParams> attachParamsCaptor =
                ArgumentCaptor.forClass(ThinWebViewAttachParams.class);
        verify(mThinWebView).attachWebContents(any(), any(), attachParamsCaptor.capture());
        SelectionDropdownMenuDelegate wrappedDelegate =
                attachParamsCaptor.getValue().selectionDropdownMenuDelegate;
        assertNotNull(wrappedDelegate);

        assertTrue(mContentView.requestFocus());

        // 1. Show the dropdown menu.
        Runnable dismissCallback = mock(Runnable.class);
        wrappedDelegate.show(null, null, null, null, dismissCallback, 0, 0);

        // Verify that the delegate's show is called.
        verify(mSelectionDropdownMenuDelegate)
                .show(any(), any(), any(), any(), any(), eq(0), eq(0));

        // 2. Trigger window focus loss. Since the dropdown is showing, it should NOT clear focus.
        mActivityController.windowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        assertTrue(mContentView.isFocused());

        // 3. Dismiss the dropdown menu when window does NOT have focus. It should clear focus.
        assertFalse(mContentView.hasWindowFocus());
        wrappedDelegate.dismiss();
        verify(mSelectionDropdownMenuDelegate).dismiss();
        assertFalse(mContentView.isFocused());
    }

    @Test
    public void testSelectionDropdownWrapper_callbackResetsIgnoreClearFocus() {
        mWebUi.setWebContents(mWebContents, true);
        attachToActivityWithFocusableSibling(mContentView);

        // Capture the wrapped delegate.
        ArgumentCaptor<ThinWebViewAttachParams> attachParamsCaptor =
                ArgumentCaptor.forClass(ThinWebViewAttachParams.class);
        verify(mThinWebView).attachWebContents(any(), any(), attachParamsCaptor.capture());
        SelectionDropdownMenuDelegate wrappedDelegate =
                attachParamsCaptor.getValue().selectionDropdownMenuDelegate;

        assertTrue(mContentView.requestFocus());

        // 1. Show the dropdown menu.
        Runnable dismissCallback = mock(Runnable.class);
        wrappedDelegate.show(null, null, null, null, dismissCallback, 0, 0);

        // Capture the wrapped callback.
        ArgumentCaptor<Runnable> wrappedCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mSelectionDropdownMenuDelegate)
                .show(any(), any(), any(), any(), wrappedCallbackCaptor.capture(), eq(0), eq(0));
        Runnable wrappedCallback = wrappedCallbackCaptor.getValue();

        // 2. Trigger the callback.
        wrappedCallback.run();
        verify(dismissCallback).run();

        // 3. Trigger window focus loss. Since the callback has run, it should clear focus.
        mActivityController.windowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        assertFalse(mContentView.isFocused());
    }

    @Test
    public void testSetWebContents_ItemDelegate_EphemeralTabOpener() {
        ContextMenuPopulatorFactory mockFactory = mock(ContextMenuPopulatorFactory.class);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        @SuppressWarnings("unchecked")
        BiConsumer<GURL, String> mockOpener = mock(BiConsumer.class);

        TabBottomSheetWebUi webUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mockFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.SIDE_PANEL,
                        mockOpener,
                        /* readLaterOpener= */ null,
                        mContentView);
        webUi.setWebContents(mWebContents, true);

        ArgumentCaptor<ContextMenuItemDelegate> captor =
                ArgumentCaptor.forClass(ContextMenuItemDelegate.class);
        verify(mockFactory).setItemDelegate(captor.capture());

        ContextMenuItemDelegate delegate = captor.getValue();
        assertNotNull(delegate);
        assertTrue(delegate instanceof ThinWebViewContextMenuItemDelegate);

        GURL testUrl = new GURL("https://example.com/image.jpg");
        String testTitle = "Test Image";
        delegate.onOpenInEphemeralTab(testUrl, testTitle, /* additionalNavigationParams= */ null);

        verify(mockOpener).accept(eq(testUrl), eq(testTitle));
    }

    @Test
    public void testSetWebContents_ItemDelegate_ReadLaterOpener() {
        ContextMenuPopulatorFactory mockFactory = mock(ContextMenuPopulatorFactory.class);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        @SuppressWarnings("unchecked")
        BiConsumer<GURL, String> mockOpener = mock(BiConsumer.class);

        TabBottomSheetWebUi webUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mockFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.UNKNOWN,
                        CoBrowseContainerType.SIDE_PANEL,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ mockOpener,
                        mContentView);
        webUi.setWebContents(mWebContents, true);

        ArgumentCaptor<ContextMenuItemDelegate> captor =
                ArgumentCaptor.forClass(ContextMenuItemDelegate.class);
        verify(mockFactory).setItemDelegate(captor.capture());

        ContextMenuItemDelegate delegate = captor.getValue();
        assertNotNull(delegate);
        assertTrue(delegate instanceof ThinWebViewContextMenuItemDelegate);

        GURL testUrl = new GURL("https://example.com/image.jpg");
        String testTitle = "Test Image";
        delegate.onReadLater(testUrl, testTitle);

        verify(mockOpener).accept(eq(testUrl), eq(testTitle));
    }

    @Test
    public void testSetWebContents_ContextualTasks_BlocksActionModeMenu() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        TestTabBottomSheetWebUi webUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ null,
                        mContentView);
        webUi.setWebContents(mWebContents, true);

        assertTrue(webUi.isDisableActionModeSelectionMenuCalled());
    }

    @Test
    public void testSetWebContents_Glic_DoesNotBlockActionModeMenu() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        View containerView =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.context_sharing.R.layout
                                        .tab_bottom_sheet,
                                null);
        TestTabBottomSheetWebUi webUi =
                new TestTabBottomSheetWebUi(
                        context,
                        containerView,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        mSelectionDropdownMenuDelegate,
                        Color.WHITE,
                        Color.LTGRAY,
                        TabBottomSheetClientType.GLIC,
                        CoBrowseContainerType.BOTTOM_SHEET,
                        /* ephemeralTabOpener= */ null,
                        /* readLaterOpener= */ null,
                        mContentView);
        webUi.setWebContents(mWebContents, true);

        assertFalse(webUi.isDisableActionModeSelectionMenuCalled());
    }

    @Test
    public void
            testTabBottomSheetWebUiContainer_dispatchDragEvent_requestsAndReleasesPermissions() {
        DragAndDropActivity activity =
                Robolectric.buildActivity(DragAndDropActivity.class).setup().get();
        TabBottomSheetWebUiContainer container = new TabBottomSheetWebUiContainer(activity, null);

        DragAndDropPermissions mockPermissions1 = mock(DragAndDropPermissions.class);
        DragAndDropPermissions mockPermissions2 = mock(DragAndDropPermissions.class);
        DragEvent dragStartedEvent = mock(DragEvent.class);
        when(dragStartedEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);

        DragEvent dropEvent1 = mock(DragEvent.class);
        when(dropEvent1.getAction()).thenReturn(DragEvent.ACTION_DROP);
        activity.mPermissionsForEvent.put(dropEvent1, mockPermissions1);

        DragEvent dropEvent2 = mock(DragEvent.class);
        when(dropEvent2.getAction()).thenReturn(DragEvent.ACTION_DROP);
        activity.mPermissionsForEvent.put(dropEvent2, mockPermissions2);

        container.dispatchDragEvent(dragStartedEvent);
        assertEquals(List.of(), activity.mRequestedEvents);

        container.dispatchDragEvent(dropEvent1);
        assertEquals(List.of(dropEvent1), activity.mRequestedEvents);
        verify(mockPermissions1, times(0)).release();

        // Consecutive drop releases previous permissions.
        container.dispatchDragEvent(dropEvent2);
        verify(mockPermissions1, times(1)).release();
        assertEquals(List.of(dropEvent1, dropEvent2), activity.mRequestedEvents);
        verify(mockPermissions2, times(0)).release();

        // A new drag session starting releases previous drag permissions.
        container.dispatchDragEvent(dragStartedEvent);
        verify(mockPermissions2, times(1)).release();
    }

    @Test
    public void testTabBottomSheetWebUiContainer_onDetachedFromWindow_releasesPermissions() {
        DragAndDropActivity activity =
                Robolectric.buildActivity(DragAndDropActivity.class).setup().get();
        TabBottomSheetWebUiContainer container = new TabBottomSheetWebUiContainer(activity, null);

        DragAndDropPermissions mockPermissions = mock(DragAndDropPermissions.class);
        DragEvent dropEvent = mock(DragEvent.class);
        when(dropEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        activity.mPermissionsForEvent.put(dropEvent, mockPermissions);

        container.dispatchDragEvent(dropEvent);
        assertEquals(List.of(dropEvent), activity.mRequestedEvents);
        verify(mockPermissions, times(0)).release();

        container.onDetachedFromWindow();
        verify(mockPermissions, times(1)).release();
    }

    @Test
    public void testSetWebContents_forwardsPermissionDelegate() {
        WindowAndroid thinWindow = mock(WindowAndroid.class);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(thinWindow);

        mWebUi.setWebContents(mWebContents, true);

        verify(thinWindow).setAndroidPermissionDelegate(mWindowAndroid);
    }

    @Test
    public void testSetWebContents_forwardsTabModelSelectorSupplier() {
        WindowAndroid thinWindow = mock(WindowAndroid.class);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(thinWindow);

        UnownedUserDataHost hostWindowHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(hostWindowHost);

        UnownedUserDataHost thinWindowHost = new UnownedUserDataHost();
        when(thinWindow.getUnownedUserDataHost()).thenReturn(thinWindowHost);

        MonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic(mock(TabModelSelector.class));
        TabModelSelectorSupplier.attach(hostWindowHost, selectorSupplier);

        mWebUi.setWebContents(mWebContents, true);

        assertEquals(selectorSupplier, TabModelSelectorSupplier.from(thinWindow));
    }

    /**
     * Attaches {@code view} to {@link #mActivity}'s window, after a focusable sibling. When {@code
     * view} clears its focus, the framework may re-assign focus to the first focusable view in the
     * window; the sibling ensures that this isn't {@code view} itself.
     */
    private void attachToActivityWithFocusableSibling(View view) {
        FrameLayout root = new FrameLayout(mActivity);
        root.addView(createFocusableView(mActivity));
        root.addView(view);
        mActivity.setContentView(root);
    }

    private static View createFocusableView(Context context) {
        View view = new View(context);
        view.setFocusable(true);
        view.setFocusableInTouchMode(true);
        return view;
    }

    /** Robolectric does not shadow requestDragAndDropPermissions(), so canned values are used. */
    private static class DragAndDropActivity extends Activity {
        final Map<DragEvent, DragAndDropPermissions> mPermissionsForEvent = new HashMap<>();
        final List<DragEvent> mRequestedEvents = new ArrayList<>();

        @Override
        public DragAndDropPermissions requestDragAndDropPermissions(DragEvent event) {
            mRequestedEvents.add(event);
            return mPermissionsForEvent.get(event);
        }
    }

    private static class TestTabBottomSheetWebUi extends TabBottomSheetWebUi {
        private final ContentView mContentView;
        private boolean mDisableActionModeSelectionMenuCalled;

        TestTabBottomSheetWebUi(
                Context context,
                View containerView,
                WindowAndroid windowAndroid,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory,
                SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
                int backgroundColor,
                int placeholderElemColor,
                @TabBottomSheetClientType int clientType,
                @CoBrowseContainerType int containerType,
                @Nullable BiConsumer<GURL, String> ephemeralTabOpener,
                @Nullable BiConsumer<GURL, String> readLaterOpener,
                ContentView contentView) {
            super(
                    context,
                    containerView,
                    windowAndroid,
                    contextMenuPopulatorFactory,
                    selectionDropdownMenuDelegate,
                    backgroundColor,
                    placeholderElemColor,
                    clientType,
                    containerType,
                    ephemeralTabOpener,
                    readLaterOpener,
                    mock(CoBrowseComponentProvider.class));
            mContentView = contentView;
        }

        @Override
        ContentView createContentView(Context context, WebContents webContents) {
            return mContentView;
        }

        @Override
        void disableActionModeSelectionMenu(WebContents webContents) {
            mDisableActionModeSelectionMenuCalled = true;
        }

        boolean isDisableActionModeSelectionMenuCalled() {
            return mDisableActionModeSelectionMenuCalled;
        }
    }
}
