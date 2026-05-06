// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

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
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;

/** Unit tests for {@link CoBrowseViews}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CoBrowseViewsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabBottomSheetWebUi mWebUi;
    @Mock private ContextualTasksFusebox mFusebox;
    @Mock private View mWebUiView;
    @Mock private View mFuseboxView;
    @Mock private View mPeekView;
    @Mock private WebContents mWebContents;
    @Mock private EventForwarder mEventForwarder;

    private Context mContext;
    private CoBrowseViews mCoBrowseViews;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mWebUi.getWebUiView()).thenReturn(mWebUiView);
        when(mFusebox.getFuseboxView()).thenReturn(mFuseboxView);
        when(mWebContents.getEventForwarder()).thenReturn(mEventForwarder);

        View rootView = LayoutInflater.from(mContext).inflate(R.layout.tab_bottom_sheet, null);
        mCoBrowseViews =
                new CoBrowseViews(
                        rootView,
                        TabBottomSheetClientType.CONTEXTUAL_TASKS,
                        mWebUi,
                        mFusebox,
                        Color.WHITE);
    }

    @Test
    public void testConstructor_BuildsViewHierarchy() {
        View view = mCoBrowseViews.getView();
        assertNotNull(view);

        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = view.findViewById(R.id.fusebox_container);

        assertEquals(1, webUiContainer.getChildCount());
        assertEquals(mWebUiView, webUiContainer.getChildAt(0));

        assertEquals(1, fuseboxContainer.getChildCount());
        assertEquals(mFuseboxView, fuseboxContainer.getChildAt(0));
    }

    @Test
    public void testDestroy() {
        mCoBrowseViews.destroy();

        verify(mWebUi).destroy();
        verify(mFusebox).destroy();

        View view = mCoBrowseViews.getView();
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = view.findViewById(R.id.fusebox_container);

        assertEquals(0, webUiContainer.getChildCount());
        assertEquals(0, fuseboxContainer.getChildCount());
    }

    @Test
    public void testSetWebUiTouchHandler() {
        TabBottomSheetWebUiContainer.TouchHandler handler =
                mock(TabBottomSheetWebUiContainer.TouchHandler.class);
        mCoBrowseViews.setWebUiTouchHandler(handler);
        // Verifies that it doesn't crash.
    }

    @Test
    public void testAttachAndRemovePeekView() {
        mCoBrowseViews.attachPeekView(mPeekView);
        assertTrue(mCoBrowseViews.hasPeekView());

        View view = mCoBrowseViews.getView();
        ViewGroup peekContainer = view.findViewById(R.id.actor_control_container);
        assertEquals(1, peekContainer.getChildCount());
        assertEquals(mPeekView, peekContainer.getChildAt(0));

        mCoBrowseViews.removePeekView(mPeekView);
        assertTrue(!mCoBrowseViews.hasPeekView());
        assertEquals(0, peekContainer.getChildCount());
    }

    @Test
    public void testSetWebContents() {
        mCoBrowseViews.setWebContents(mWebContents);
        verify(mWebUi).setWebContents(mWebContents);
    }

    @Test
    public void testSetWebContents_UpdatesViewWhenChanged() {
        View newWebUiView = new View(mContext);
        when(mWebUi.getWebUiView()).thenReturn(mWebUiView).thenReturn(newWebUiView);

        mCoBrowseViews.setWebContents(mWebContents);

        View view = mCoBrowseViews.getView();
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);
        assertEquals(1, webUiContainer.getChildCount());
        assertEquals(newWebUiView, webUiContainer.getChildAt(0));
    }
}
