// Copyright 2017 The Chromium Authors
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
import org.chromium.chrome.browser.infobar.IphInfoBarSupport.IphBubbleDelegate;
import org.chromium.chrome.browser.infobar.IphInfoBarSupport.PopupState;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarUiItem;

/** Tests {@link IphInfoBarSupport}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IphInfoBarSupportTest {
    @Mock private IphBubbleDelegate mDelegate;
    @Mock private InfoBarUiItem mItem;
    @Mock private View mView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @Feature({"Browser"})
    public void testUnattachedInfoBarView() {
        IphInfoBarSupport support = new IphInfoBarSupport(mDelegate);

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
        IphInfoBarSupport support = new IphInfoBarSupport(mDelegate);

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
        // TODO(crbug.com/41354193): Clang formatted this incorrectly.
        doAnswer(
                        (invocation) -> {
                            support.onDismiss();
                            return null;
                        })
                .when(bubble)
                .dismiss();

        support.notifyAllAnimationsFinished(mItem);

        // Verify that the IphInfoBarSupport tries to create and show a bubble.
        verify(mDelegate, times(1)).createStateForInfoBar(mView, 1);
        verify(bubble, times(1)).addOnDismissListener(support);
        verify(bubble, times(1)).show();

        support.onRemoveInfoBar(null, infoBar, false);

        // Verify the IphInfoBarSupport properly triggers the delegate dismiss call when the infobar
        // is gone and dismissed.
        verify(mDelegate, times(1)).onPopupDismissed(state);
    }
}
