// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link DocumentPictureInPictureHeaderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureHeaderMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private DocumentPictureInPictureHeaderDelegate mDelegate;

    private static final int DEFAULT_THEME_COLOR = Color.BLUE;
    private static final ColorStateList DEFAULT_FOCUS_TINT = ColorStateList.valueOf(Color.RED);

    private Context mContext;
    private PropertyModel mModel;
    private DocumentPictureInPictureHeaderMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mModel =
                Mockito.spy(
                        new PropertyModel.Builder(DocumentPictureInPictureHeaderProperties.ALL_KEYS)
                                .build());
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(null);
        when(mThemeColorProvider.getThemeColor()).thenReturn(DEFAULT_THEME_COLOR);
        when(mThemeColorProvider.getActivityFocusTint()).thenReturn(DEFAULT_FOCUS_TINT);
    }

    private void createMediator() {
        createMediator(/* isBackToTabShown= */ true);
    }

    private void createMediator(boolean isBackToTabShown) {
        mMediator =
                new DocumentPictureInPictureHeaderMediator(
                        mModel,
                        mDesktopWindowStateManager,
                        mThemeColorProvider,
                        mDelegate,
                        isBackToTabShown);
    }

    @Test
    @SmallTest
    public void testCreation() {
        createMediator();
        verify(mDesktopWindowStateManager).addObserver(mMediator);
        verify(mDesktopWindowStateManager).getAppHeaderState();
        verify(mThemeColorProvider).addThemeColorObserver(mMediator);
        verify(mThemeColorProvider).addTintObserver(mMediator);

        // Verify that the color is set during creation.
        verify(mDesktopWindowStateManager).updateForegroundColor(DEFAULT_THEME_COLOR);
        assertEquals(
                DEFAULT_THEME_COLOR,
                (int) mModel.get(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR));
        assertEquals(
                DEFAULT_FOCUS_TINT,
                mModel.get(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST));
        assertNotNull(
                mModel.get(DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER));
        assertNotNull(
                mModel.get(DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER));
    }

    @Test
    @SmallTest
    public void testBackToTab() {
        createMediator();
        mMediator.onBackToTab();
        verify(mDelegate).onBackToTab();
    }

    @Test
    @SmallTest
    public void testIsBackToTabHidden() {
        createMediator(/* isBackToTabShown= */ false);
        assertFalse(mModel.get(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN));
    }

    @Test
    @SmallTest
    public void testOnBackToTabClickListener() {
        createMediator();
        View.OnClickListener listener =
                mModel.get(DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER);
        listener.onClick(new View(mContext));
        verify(mDelegate).onBackToTab();
    }

    @Test
    @SmallTest
    public void testOnLayoutChangeListener() {
        createMediator();
        View.OnLayoutChangeListener listener =
                mModel.get(DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER);

        // Create a view hierarchy with the button.
        FrameLayout parent = new FrameLayout(mContext);
        View button = new View(mContext);
        button.setId(R.id.document_picture_in_picture_header_back_to_tab);
        // Set layout so getHitRect works.
        button.layout(10, 10, 30, 30);
        parent.addView(button);

        listener.onLayoutChange(parent, 0, 0, 100, 100, 0, 0, 0, 0);

        List<Rect> rects = mModel.get(DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS);
        assertNotNull(rects);
        assertEquals(1, rects.size());
        assertEquals(new Rect(10, 10, 30, 30), rects.get(0));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        createMediator();
        mMediator.destroy();
        verify(mDesktopWindowStateManager).removeObserver(mMediator);
        verify(mThemeColorProvider).removeThemeColorObserver(mMediator);
        verify(mThemeColorProvider).removeTintObserver(mMediator);
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
    public void testTintChanged() {
        createMediator();
        ColorStateList focusTint = ColorStateList.valueOf(Color.GREEN);
        mMediator.onTintChanged(/* tint= */ null, focusTint, BrandedColorScheme.APP_DEFAULT);

        assertEquals(
                focusTint, mModel.get(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST));
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
