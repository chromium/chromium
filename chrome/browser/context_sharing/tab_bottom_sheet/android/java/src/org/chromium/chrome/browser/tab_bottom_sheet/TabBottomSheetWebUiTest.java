// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

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
    @Mock private CoBrowseViewsZoomControl mZoomControl;
    @Mock private ContentView mMockContentView;
    @Mock private Window mMockWindow;
    @Mock private View mMockDecorView;
    @Mock private EventForwarder mEventForwarder;

    private TabBottomSheetWebUi mWebUi;

    @Before
    public void setUp() {
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
        when(mThinWebView.getView()).thenReturn(mView);

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
                        Color.WHITE,
                        mZoomControl,
                        mMockContentView);
        TabBottomSheetWebUi.setInTestModeForTesting();
    }

    @Test
    public void testSetWebContents_SameWebContents_Noop() {
        mWebUi.setWebContents(mWebContents);
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());

        mWebUi.setWebContents(mWebContents);
        // Verify it was not called again.
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());
    }

    @Test
    public void testSetWebContents_DifferentWebContents_Updates() {
        mWebUi.setWebContents(mWebContents);
        verify(mWebContents, times(1)).setDelegates(any(), any(), any(), eq(mWindowAndroid), any());

        WebContents secondWebContents = mock(WebContents.class);
        Mockito.doReturn(mEventForwarder).when(secondWebContents).getEventForwarder();

        mWebUi.setWebContents(secondWebContents);
        verify(secondWebContents, times(1))
                .setDelegates(any(), any(), any(), eq(mWindowAndroid), any());
    }

    @Test
    public void testSetWebContents_UpdatesDelegatesWhenAlreadySet() {
        ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(null);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewDelegate);

        mWebUi.setWebContents(mWebContents);

        verify(mWebContents, times(0)).setDelegates(any(), any(), any(), any(), any());
        verify(mWebContents, times(1)).setTopLevelNativeWindow(eq(mWindowAndroid));
        assertNotNull(viewDelegate.getContainerView());
    }

    @Test
    public void testFocusHandling() {
        ViewTreeObserver mockViewTreeObserver = mock(ViewTreeObserver.class);
        when(mMockContentView.getViewTreeObserver()).thenReturn(mockViewTreeObserver);

        mWebUi.setWebContents(mWebContents);

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

        mWebUi.setWebContents(nonNullWebContents);
        mWebUi.setWebContents(null);
        verify(mThinWebView, times(1)).destroy();
    }

    @Test
    public void testDestroy() {
        mWebUi.setWebContents(mWebContents);
        mWebUi.destroy();
        verify(mThinWebView).destroy();
        org.junit.Assert.assertNull(mWebUi.getWebContents());
    }

    @Test
    public void testGetWebUiView() {
        View view = mWebUi.getWebUiView();
        assertNotNull(view);
    }

    @Test
    public void testCreateWebContentsDelegate_ContentsZoomChange() {
        mWebUi.setWebContents(mWebContents);

        ArgumentCaptor<ThinWebViewAttachParams> paramsCaptor =
                ArgumentCaptor.forClass(ThinWebViewAttachParams.class);
        verify(mThinWebView).attachWebContents(eq(mWebContents), any(), paramsCaptor.capture());

        ThinWebViewAttachParams params = paramsCaptor.getValue();
        org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid delegate =
                params.webContentsDelegate;
        assertNotNull(delegate);

        delegate.contentsZoomChange(true);
        verify(mZoomControl).zoomIn(mWebContents);

        delegate.contentsZoomChange(false);
        verify(mZoomControl).zoomOut(mWebContents);
    }

    @Test
    public void testSetWebContents_resetsTouchOffset() {
        mWebUi.setWebContents(mWebContents);

        verify(mEventForwarder).setCurrentTouchOffsetX(0.0f);
        verify(mEventForwarder).setCurrentTouchOffsetY(0.0f);
    }

    private static class TestTabBottomSheetWebUi extends TabBottomSheetWebUi {
        private final ContentView mMockContentView;

        TestTabBottomSheetWebUi(
                Context context,
                View containerView,
                WindowAndroid windowAndroid,
                ContextMenuPopulatorFactory contextMenuPopulatorFactory,
                int backgroundColor,
                CoBrowseViewsZoomControl zoomControl,
                ContentView mockContentView) {
            super(
                    context,
                    containerView,
                    windowAndroid,
                    contextMenuPopulatorFactory,
                    backgroundColor,
                    zoomControl);
            mMockContentView = mockContentView;
        }

        @Override
        ContentView createContentView(Context context, WebContents webContents) {
            return mMockContentView;
        }
    }
}
