// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link DocumentPictureInPictureHeaderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private ThemeColorProvider mThemeColorProvider;

    private static final int DEFAULT_THEME_COLOR = Color.BLUE;

    private PropertyModel mModel;
    private DocumentPictureInPictureHeaderMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                Mockito.spy(
                        new PropertyModel.Builder(DocumentPictureInPictureHeaderProperties.ALL_KEYS)
                                .build());
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        when(mThemeColorProvider.getThemeColor()).thenReturn(DEFAULT_THEME_COLOR);
    }

    private void createMediator() {
        mMediator =
                new DocumentPictureInPictureHeaderMediator(
                        mModel, mDesktopWindowStateManager, mThemeColorProvider);
    }

    @Test
    @SmallTest
    public void testCreation() {
        createMediator();
        verify(mDesktopWindowStateManager).addObserver(mMediator);
        verify(mDesktopWindowStateManager).getAppHeaderState();
        verify(mThemeColorProvider).addThemeColorObserver(mMediator);

        // Verify that the color is set during creation.
        verify(mDesktopWindowStateManager).updateForegroundColor(DEFAULT_THEME_COLOR);
        assertEquals(
                DEFAULT_THEME_COLOR,
                (int) mModel.get(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        createMediator();
        mMediator.destroy();
        verify(mDesktopWindowStateManager).removeObserver(mMediator);
        verify(mThemeColorProvider).removeThemeColorObserver(mMediator);
    }

    @Test
    @SmallTest
    public void testStateChanged_NotInDesktopWindow() {
        createMediator();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertFalse(mModel.get(DocumentPictureInPictureHeaderProperties.IS_SHOWN));
    }

    @Test
    @SmallTest
    public void testStateChanged_InDesktopWindow() {
        createMediator();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(100);
        when(mAppHeaderState.getLeftPadding()).thenReturn(10);
        when(mAppHeaderState.getRightPadding()).thenReturn(20);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertTrue(mModel.get(DocumentPictureInPictureHeaderProperties.IS_SHOWN));
        assertEquals(100, (int) mModel.get(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT));
        assertEquals(10, mModel.get(DocumentPictureInPictureHeaderProperties.HEADER_SPACING).left);
        assertEquals(20, mModel.get(DocumentPictureInPictureHeaderProperties.HEADER_SPACING).right);
        assertEquals(0, mModel.get(DocumentPictureInPictureHeaderProperties.HEADER_SPACING).top);
        assertEquals(0, mModel.get(DocumentPictureInPictureHeaderProperties.HEADER_SPACING).bottom);
    }

    @Test
    @SmallTest
    public void testThemeColorChanged() {
        createMediator();
        int color = Color.RED;
        mMediator.onThemeColorChanged(color, /* shouldAnimate= */ false);

        assertEquals(
                color, (int) mModel.get(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR));
        verify(mDesktopWindowStateManager).updateForegroundColor(color);
    }

    @Test
    @SmallTest
    public void testStateChanged_Dedupe() {
        createMediator();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        // Reset interactions to ensure it doesn't fire again for the same state object.
        Mockito.clearInvocations(mModel);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        verify(mModel, never())
                .set(eq(DocumentPictureInPictureHeaderProperties.IS_SHOWN), anyBoolean());
        verify(mModel, never())
                .set(eq(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT), anyInt());
        verify(mModel, never())
                .set(eq(DocumentPictureInPictureHeaderProperties.HEADER_SPACING), any());
    }
}
