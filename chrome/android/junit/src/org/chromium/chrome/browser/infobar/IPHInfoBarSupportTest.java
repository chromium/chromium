// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.IBinder;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.IPHBubbleDelegate;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.PopupState;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;

/** Tests {@link IPHInfoBarSupport}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IPHInfoBarSupportTest {
    @Mock
    private IPHBubbleDelegate mDelegate;
    @Mock
    private Item mItem;
    @Mock
    private View mView;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @Feature({"Browser"})
    public void testUnattachedInfoBarView() {
        IPHInfoBarSupport support = new IPHInfoBarSupport(mDelegate);

        when(mView.getWindowToken()).thenReturn(null);
        when(mView.isAttachedToWindow()).thenReturn(false);
        when(mItem.getView()).thenReturn(mView);
        when(mItem.getInfoBarIdentifier()).thenReturn(1);
        when(mDelegate.createStateForInfoBar(any(), anyInt())).thenReturn(null);

        support.notifyAllAnimationsFinished(mItem);

        // Verify that we never try to show or log an event when the View for the infobar is not
        // attached to the view hierarchy.
        verify(mDelegate, never()).createStateForInfoBar(any(), anyInt());
    }

    @Test
    @Feature({"Browser"})
    public void testBasicCallPath() {
        IPHInfoBarSupport support = new IPHInfoBarSupport(mDelegate);

        IBinder windowToken = mock(IBinder.class);
        TextBubble bubble = mock(TextBubble.class);
        InfoBar infoBar = mock(InfoBar.class);

        PopupState state = new PopupState();
        state.bubble = bubble;
        state.view = mView;
        state.feature = "Test Feature";

        when(mView.getWindowToken()).thenReturn(windowToken);
        when(mView.isAttachedToWindow()).thenReturn(true);
        when(mItem.getView()).thenReturn(mView);
        when(mItem.getInfoBarIdentifier()).thenReturn(1);
        when(mDelegate.createStateForInfoBar(mView, 1)).thenReturn(state);
        when(infoBar.getView()).thenReturn(mView);
        // clang-format off
        // TODO(crbug.com/782796): Clang formatted this incorrectly.
        doAnswer((invocation) -> { support.onDismiss(); return null; }).when(bubble).dismiss();
        // clang-format on

        support.notifyAllAnimationsFinished(mItem);

        // Verify that the IPHInfoBarSupport tries to create and show a bubble.
        verify(mDelegate, times(1)).createStateForInfoBar(mView, 1);
        verify(bubble, times(1)).addOnDismissListener(support);
        verify(bubble, times(1)).show();

        support.onRemoveInfoBar(null, infoBar, false);

        // Verify the IPHInfoBarSupport properly triggers the delegate dismiss call when the infobar
        // is gone and dismissed.
        verify(mDelegate, times(1)).onPopupDismissed(state);
    }
}