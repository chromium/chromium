// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link DesktopPopupHeaderMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DesktopPopupHeaderMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private Tab mTab;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private Context mContext;
    private PropertyModel mModel;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private DesktopPopupHeaderMediator mMediator;
    private int mMinTitleWidth;
    private int mMinHeaderHeight;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mModel = new PropertyModel.Builder(DesktopPopupHeaderProperties.ALL_KEYS).build();
        mTabSupplier = new ObservableSupplierImpl<>();

        final Resources res = mContext.getResources();
        mMinTitleWidth = res.getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_min_width);
        mMinHeaderHeight =
                Math.max(
                        res.getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_min_height),
                        res.getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_text_height));

        mMediator =
                new DesktopPopupHeaderMediator(
                        mModel,
                        mDesktopWindowStateManager,
                        mTabSupplier,
                        mContext,
                        /* isIncognito= */ false);
    }

    @Test
    @SmallTest
    public void testCreation() {
        verify(mDesktopWindowStateManager).addObserver(mMediator);
        assertEquals(
                ChromeColors.getLargeTextPrimaryStyle(false),
                (int) mModel.get(DesktopPopupHeaderProperties.TITLE_APPEARANCE));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mMediator.destroy();
        verify(mDesktopWindowStateManager).removeObserver(mMediator);
    }

    @Test
    @SmallTest
    public void testStateChanged_NotInDesktopWindow() {
        // Setup state: Not in desktop window.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertFalse(mModel.get(DesktopPopupHeaderProperties.IS_SHOWN));
    }

    @Test
    @SmallTest
    public void testStateChanged_InDesktopWindow_WideAndTallEnough() {
        // Setup state: In desktop, wide enough for title.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(mMinTitleWidth + 10);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(mMinHeaderHeight + 50);
        when(mAppHeaderState.getLeftPadding()).thenReturn(10);
        when(mAppHeaderState.getRightPadding()).thenReturn(20);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertTrue(mModel.get(DesktopPopupHeaderProperties.IS_SHOWN));
        assertTrue(mModel.get(DesktopPopupHeaderProperties.TITLE_VISIBLE));
        assertEquals(
                mMinHeaderHeight + 50,
                (int) mModel.get(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX));
        assertEquals(10, mModel.get(DesktopPopupHeaderProperties.TITLE_SPACING).left);
        assertEquals(20, mModel.get(DesktopPopupHeaderProperties.TITLE_SPACING).right);
        assertEquals(0, mModel.get(DesktopPopupHeaderProperties.TITLE_SPACING).top);
        assertEquals(0, mModel.get(DesktopPopupHeaderProperties.TITLE_SPACING).bottom);

        int expectedColor = ChromeColors.getDefaultBgColor(mContext, false);
        assertEquals(
                expectedColor, (int) mModel.get(DesktopPopupHeaderProperties.BACKGROUND_COLOR));
        verify(mDesktopWindowStateManager).updateForegroundColor(expectedColor);
    }

    @Test
    @SmallTest
    public void testStateChanged_InDesktopWindow_NotWideEnough() {
        // Setup state: In desktop, NOT wide enough for title.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(mMinTitleWidth - 1);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(mMinHeaderHeight);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertTrue(mModel.get(DesktopPopupHeaderProperties.IS_SHOWN));
        // Title should be hidden.
        assertFalse(mModel.get(DesktopPopupHeaderProperties.TITLE_VISIBLE));
    }

    @Test
    @SmallTest
    public void testStateChanged_InDesktopWindow_NotTallEnough() {
        // Setup state: In desktop, app header too short.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(mMinTitleWidth);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(mMinHeaderHeight - 1);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        assertTrue(mModel.get(DesktopPopupHeaderProperties.IS_SHOWN));
        assertTrue(mModel.get(DesktopPopupHeaderProperties.TITLE_VISIBLE));
        assertEquals(
                mMinHeaderHeight, (int) mModel.get(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX));
    }

    @Test
    @SmallTest
    public void testStateChanged_Dedupe() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);

        // Reset interactions to ensure it doesn't fire again for the same state object.
        org.mockito.Mockito.clearInvocations(mDesktopWindowStateManager);

        mMediator.onAppHeaderStateChanged(mAppHeaderState);
        verify(mDesktopWindowStateManager, never()).updateForegroundColor(anyInt());
    }

    @Test
    @SmallTest
    public void testTabTitleUpdate() {
        String title = "My Tab Title";
        doReturn(title).when(mTab).getTitle();

        // Setting the tab should trigger the TabSupplierObserver, which calls
        // onTitleUpdated.
        mTabSupplier.set(mTab);

        assertEquals(title, mModel.get(DesktopPopupHeaderProperties.TITLE_TEXT));

        // Simulate a title change.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        String newTitle = "New Title";
        doReturn(newTitle).when(mTab).getTitle();
        mTabObserverCaptor.getValue().onTitleUpdated(mTab);

        assertEquals(newTitle, mModel.get(DesktopPopupHeaderProperties.TITLE_TEXT));
    }
}
