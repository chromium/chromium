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
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
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

    private TabBottomSheetWebUi mWebUi;

    @Before
    public void setUp() {
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
        when(mThinWebView.getView()).thenReturn(mView);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mWebUi =
                new TabBottomSheetWebUi(
                        context,
                        mWindowAndroid,
                        mContextMenuPopulatorFactory,
                        Color.WHITE,
                        new CoBrowseViewsZoomControl() {
                            @Override
                            public boolean zoomIn(WebContents webContents) {
                                return false;
                            }

                            @Override
                            public boolean zoomOut(WebContents webContents) {
                                return false;
                            }
                        });
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
}
