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

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;

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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
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

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabBottomSheetWebUi}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetWebUiTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WebContents mWebContents;
    @Mock private ThinWebView mThinWebView;
    @Mock private View mView;
    @Mock private ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    @Mock private SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
    @Mock private ContentView mMockContentView;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;
    @Mock private EventForwarder mEventForwarder;
    @Mock private Activity mMockActivity;

    private TabBottomSheetWebUi mWebUi;

    @Before
    public void setUp() {
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
        when(mThinWebView.getView()).thenReturn(mView);

        WeakReference<Activity> weakActivity = new WeakReference<>(mMockActivity);
        when(mWindowAndroid.getActivity()).thenReturn(weakActivity);

        when(mWindowAndroid.getWindow()).thenReturn(mMockWindow);
        when(mMockWindow.getDecorView()).thenReturn(mMockDecorView);
        when(mMockDecorView.getHeight()).thenReturn(1000);

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
                        mMockContentView);
        TabBottomSheetWebUi.setInTestModeForTesting();
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
        ViewTreeObserver mockViewTreeObserver = mock(ViewTreeObserver.class);
        when(mMockContentView.getViewTreeObserver()).thenReturn(mockViewTreeObserver);

        mWebUi.setWebContents(mWebContents, true);

        ArgumentCaptor<View.OnAttachStateChangeListener> attachListenerCaptor =
                ArgumentCaptor.forClass(View.OnAttachStateChangeListener.class);
        verify(mMockContentView).addOnAttachStateChangeListener(attachListenerCaptor.capture());
        View.OnAttachStateChangeListener attachListener = attachListenerCaptor.getValue();
        assertNotNull(attachListener);

        attachListener.onViewAttachedToWindow(mMockContentView);

        ArgumentCaptor<ViewTreeObserver.OnWindowFocusChangeListener> focusListenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnWindowFocusChangeListener.class);
        verify(mockViewTreeObserver).addOnWindowFocusChangeListener(focusListenerCaptor.capture());
        ViewTreeObserver.OnWindowFocusChangeListener focusListener = focusListenerCaptor.getValue();
        assertNotNull(focusListener);

        focusListener.onWindowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        verify(mMockContentView).clearFocus();

        Mockito.reset(mMockContentView);
        when(mMockContentView.getViewTreeObserver()).thenReturn(mockViewTreeObserver);

        focusListener.onWindowFocusChanged(true);
        verify(mMockContentView, times(0)).clearFocus();

        attachListener.onViewDetachedFromWindow(mMockContentView);
        verify(mockViewTreeObserver).removeOnWindowFocusChangeListener(focusListener);
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

        Activity mockActivity = mock(Activity.class);
        when(mockActivity.isDestroyed()).thenReturn(true);
        WeakReference<Activity> weakActivity = new WeakReference<>(mockActivity);
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
        assertNull(mWebUi.getWebContents());
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
        Activity mockActivity = mMockActivity;
        assertNotNull(mockActivity);
        View focusedView = mock();
        when(mockActivity.getCurrentFocus()).thenReturn(focusedView);

        mWebUi.setWebContents(mWebContents, true);

        verify(focusedView, times(1)).clearFocus();
    }

    @Test
    public void testSetWebContents_noFocus_doesNotClearActivityFocus() {
        Activity mockActivity = mMockActivity;
        assertNotNull(mockActivity);
        View focusedView = mock();
        when(mockActivity.getCurrentFocus()).thenReturn(focusedView);

        mWebUi.setWebContents(mWebContents, false);

        verify(focusedView, times(0)).clearFocus();
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
                        mMockContentView,
                        CoBrowseContainerType.SIDE_PANEL);
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
    public void testSelectionDropdownWrapper_ignoresFocusClearWhenShowing() {
        ViewTreeObserver mockViewTreeObserver = mock(ViewTreeObserver.class);
        when(mMockContentView.getViewTreeObserver()).thenReturn(mockViewTreeObserver);

        mWebUi.setWebContents(mWebContents, true);

        // Capture the wrapped delegate.
        ArgumentCaptor<ThinWebViewAttachParams> attachParamsCaptor =
                ArgumentCaptor.forClass(ThinWebViewAttachParams.class);
        verify(mThinWebView).attachWebContents(any(), any(), attachParamsCaptor.capture());
        SelectionDropdownMenuDelegate wrappedDelegate =
                attachParamsCaptor.getValue().selectionDropdownMenuDelegate;
        assertNotNull(wrappedDelegate);

        // Capture the OnWindowFocusChangeListener.
        ArgumentCaptor<View.OnAttachStateChangeListener> attachListenerCaptor =
                ArgumentCaptor.forClass(View.OnAttachStateChangeListener.class);
        verify(mMockContentView).addOnAttachStateChangeListener(attachListenerCaptor.capture());
        View.OnAttachStateChangeListener attachListener = attachListenerCaptor.getValue();
        attachListener.onViewAttachedToWindow(mMockContentView);

        ArgumentCaptor<ViewTreeObserver.OnWindowFocusChangeListener> focusListenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnWindowFocusChangeListener.class);
        verify(mockViewTreeObserver).addOnWindowFocusChangeListener(focusListenerCaptor.capture());
        ViewTreeObserver.OnWindowFocusChangeListener focusListener = focusListenerCaptor.getValue();

        // 1. Show the dropdown menu.
        Runnable dismissCallback = mock(Runnable.class);
        wrappedDelegate.show(null, null, null, null, dismissCallback, 0, 0);

        // Verify that the delegate's show is called.
        verify(mSelectionDropdownMenuDelegate)
                .show(any(), any(), any(), any(), any(), eq(0), eq(0));

        // 2. Trigger window focus loss. Since the dropdown is showing, it should NOT clear focus.
        Mockito.reset(mMockContentView);
        focusListener.onWindowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        verify(mMockContentView, times(0)).clearFocus();

        // 3. Dismiss the dropdown menu when window does NOT have focus. It should clear focus.
        when(mMockContentView.hasWindowFocus()).thenReturn(false);
        wrappedDelegate.dismiss();
        verify(mSelectionDropdownMenuDelegate).dismiss();
        verify(mMockContentView, times(1)).clearFocus();
    }

    @Test
    public void testSelectionDropdownWrapper_callbackResetsIgnoreClearFocus() {
        ViewTreeObserver mockViewTreeObserver = mock(ViewTreeObserver.class);
        when(mMockContentView.getViewTreeObserver()).thenReturn(mockViewTreeObserver);

        mWebUi.setWebContents(mWebContents, true);

        // Capture the wrapped delegate.
        ArgumentCaptor<ThinWebViewAttachParams> attachParamsCaptor =
                ArgumentCaptor.forClass(ThinWebViewAttachParams.class);
        verify(mThinWebView).attachWebContents(any(), any(), attachParamsCaptor.capture());
        SelectionDropdownMenuDelegate wrappedDelegate =
                attachParamsCaptor.getValue().selectionDropdownMenuDelegate;

        // Capture the OnWindowFocusChangeListener.
        ArgumentCaptor<View.OnAttachStateChangeListener> attachListenerCaptor =
                ArgumentCaptor.forClass(View.OnAttachStateChangeListener.class);
        verify(mMockContentView).addOnAttachStateChangeListener(attachListenerCaptor.capture());
        View.OnAttachStateChangeListener attachListener = attachListenerCaptor.getValue();
        attachListener.onViewAttachedToWindow(mMockContentView);

        ArgumentCaptor<ViewTreeObserver.OnWindowFocusChangeListener> focusListenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnWindowFocusChangeListener.class);
        verify(mockViewTreeObserver).addOnWindowFocusChangeListener(focusListenerCaptor.capture());
        ViewTreeObserver.OnWindowFocusChangeListener focusListener = focusListenerCaptor.getValue();

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
        Mockito.reset(mMockContentView);
        focusListener.onWindowFocusChanged(false);
        ShadowLooper.idleMainLooper();
        verify(mMockContentView, times(1)).clearFocus();
    }

    private static class TestTabBottomSheetWebUi extends TabBottomSheetWebUi {
        private final ContentView mMockContentView;

        TestTabBottomSheetWebUi(
                Context context,
                View containerView,
                WindowAndroid windowAndroid,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory,
                SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
                int backgroundColor,
                ContentView mockContentView) {
            this(
                    context,
                    containerView,
                    windowAndroid,
                    contextMenuPopulatorFactory,
                    selectionDropdownMenuDelegate,
                    backgroundColor,
                    mockContentView,
                    CoBrowseContainerType.BOTTOM_SHEET);
        }

        TestTabBottomSheetWebUi(
                Context context,
                View containerView,
                WindowAndroid windowAndroid,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory,
                SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
                int backgroundColor,
                ContentView mockContentView,
                @CoBrowseContainerType int containerType) {
            super(
                    context,
                    containerView,
                    windowAndroid,
                    contextMenuPopulatorFactory,
                    selectionDropdownMenuDelegate,
                    backgroundColor,
                    containerType);
            mMockContentView = mockContentView;
        }

        @Override
        ContentView createContentView(Context context, WebContents webContents) {
            return mMockContentView;
        }
    }
}
