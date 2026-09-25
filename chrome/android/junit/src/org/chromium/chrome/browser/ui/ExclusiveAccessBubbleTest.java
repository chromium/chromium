// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Tests for {@link ExclusiveAccessBubble}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExclusiveAccessBubbleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExclusiveAccessContext mExclusiveAccessContext;
    @Mock private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() {
        Mockito.when(mExclusiveAccessContext.getSnackbarManager()).thenReturn(mSnackbarManager);
    }

    @Test
    public void testBubbleShowAndHide() {
        ExclusiveAccessBubble bubble = ExclusiveAccessBubble.create(mExclusiveAccessContext);
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);

        Assert.assertFalse(bubble.isVisible());
        bubble.update("Test text");
        Assert.assertTrue(bubble.isVisible());
        Mockito.verify(mSnackbarManager, Mockito.times(1)).showSnackbar(snackbarCaptor.capture());
        Assert.assertTrue(snackbarCaptor.getValue().isHighPriority());
        Assert.assertFalse(snackbarCaptor.getValue().getDefaultLines());

        // Dynamic update while visible should also preserve non-default line limit.
        bubble.update("Updated text");
        Mockito.verify(mSnackbarManager, Mockito.times(2)).showSnackbar(snackbarCaptor.capture());
        Assert.assertFalse(snackbarCaptor.getValue().getDefaultLines());

        bubble.hide();
        Assert.assertFalse(bubble.isVisible());
        Mockito.verify(mSnackbarManager, Mockito.times(1)).dismissSnackbars(Mockito.any());
    }

    @Test
    public void testUpdateWithSameText() {
        ExclusiveAccessBubble bubble = ExclusiveAccessBubble.create(mExclusiveAccessContext);

        bubble.update("Test text");
        Mockito.verify(mSnackbarManager, Mockito.times(1)).showSnackbar(Mockito.any());

        // Update with the same text and visible snackbar should not trigger another show.
        bubble.update("Test text");
        Mockito.verify(mSnackbarManager, Mockito.times(1)).showSnackbar(Mockito.any());
        Mockito.verify(mSnackbarManager, Mockito.never()).dismissSnackbars(Mockito.any());

        // Simulate snackbar dismissal (e.g. swipe).
        bubble.hide();
        Mockito.verify(mSnackbarManager, Mockito.times(1)).dismissSnackbars(Mockito.any());

        // Note: The Java layer does not control whether the snackbar has been shown or not.
        // The actual behavior that the snackbar won't be shown is controlled by C++
        // `ExclusiveAccessBubbleAndroid::was_shown_`. This is because swiping away the Snackbar
        // triggers a layout change in Android, which can cause the WebContents to re-evaluate
        // its fullscreen state and fire another Update() from C++.
        bubble.update("Test text");
        Mockito.verify(mSnackbarManager, Mockito.times(2)).showSnackbar(Mockito.any());
    }

    @Test
    public void testUpdateWithNewTextReusesSnackbarInstance() {
        ExclusiveAccessBubble bubble = ExclusiveAccessBubble.create(mExclusiveAccessContext);

        bubble.update("Press Esc");
        ArgumentCaptor<Snackbar> captor = ArgumentCaptor.forClass(Snackbar.class);
        Mockito.verify(mSnackbarManager, Mockito.times(1)).showSnackbar(captor.capture());
        Snackbar firstSnackbar = captor.getValue();
        Assert.assertEquals("Press Esc", firstSnackbar.getTextForTesting());

        bubble.update("Press and hold Esc");
        Mockito.verify(mSnackbarManager, Mockito.times(2)).showSnackbar(captor.capture());
        Snackbar secondSnackbar = captor.getValue();

        Assert.assertSame(firstSnackbar, secondSnackbar);
        Assert.assertEquals("Press and hold Esc", secondSnackbar.getTextForTesting());
        Mockito.verify(mSnackbarManager, Mockito.never()).dismissSnackbars(Mockito.any());
    }
}
