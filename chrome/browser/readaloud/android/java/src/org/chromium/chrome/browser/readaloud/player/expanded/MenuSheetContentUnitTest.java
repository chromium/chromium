// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link ExpandedPlayerSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MenuSheetContentUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ExpandedPlayerSheetContent mBottomSheetContent;
    private Activity mActivity;
    private Context mContext;
    private Menu mMenu;

    static class TestMenuSheetContent extends MenuSheetContent {
        TestMenuSheetContent(
                BottomSheetContent parent, BottomSheetController bottomSheetController) {
            super(parent, bottomSheetController);
        }

        @Override
        public View getContentView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public int getSheetContentDescriptionStringId() {
            // "Options menu"
            // Automatically appended: "Swipe down to close."
            return R.string.readaloud_options_menu_description;
        }
    }

    private MenuSheetContent mContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContent = new TestMenuSheetContent(mBottomSheetContent, mBottomSheetController);
    }

    @Test
    public void testNotifySheetClosed() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mContent);
        mContent.notifySheetClosed(mContent);
        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, false);
    }

    @Test
    public void testOpenParent() {
        mContent.openSheet(mBottomSheetContent);
        // Hiding self will show the parent sheet.
        verify(mBottomSheetController).hideContent(mContent, false);
    }

    @Test
    public void testGetBottomSheetController() {
        assertEquals(mBottomSheetController, mContent.getBottomSheetController());
    }

    @Test
    public void testGetToolbarView() {
        assertEquals(mContent.getToolbarView(), null);
    }

    @Test
    public void testGetPriority() {
        assertEquals(mContent.getPriority(), BottomSheetContent.ContentPriority.HIGH);
    }

    @Test
    public void testSwipeToDismissEnabled() {
        assertTrue(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testHasCustomLifecycle() {
        assertFalse(mContent.hasCustomLifecycle());
    }

    @Test
    public void testHasCustomScrimLifecycle() {
        assertFalse(mContent.hasCustomScrimLifecycle());
    }

    @Test
    public void testGetPeekHeight() {
        assertEquals(mContent.getPeekHeight(), BottomSheetContent.HeightMode.DISABLED);
    }

    @Test
    public void testGetHalfHeightRatio() {
        assertEquals(mContent.getHalfHeightRatio(), BottomSheetContent.HeightMode.DISABLED, 0.01f);
    }

    @Test
    public void testGetFullHeightRatio() {
        assertEquals(
                mContent.getFullHeightRatio(), BottomSheetContent.HeightMode.WRAP_CONTENT, 0.01f);
    }

    @Test
    public void testGetSheetFullHeightAccessibilityStringId() {
        assertEquals(
                mContent.getSheetFullHeightAccessibilityStringId(),
                R.string.readaloud_player_opened_at_full_height);
    }

    @Test
    public void testGetSheetClosedAccessibilityStringId() {
        assertEquals(
                mContent.getSheetClosedAccessibilityStringId(),
                R.string.readaloud_player_minimized);
    }

    @Test
    public void testHandleBackPress() {
        mContent.handleBackPress();
        verify(mBottomSheetController).hideContent(mContent, false);
    }

    @Test
    public void testGetBackPressStateChangedSupplier() {
        ObservableSupplierImpl<Boolean> supplier = mContent.getBackPressStateChangedSupplier();
        assertTrue(supplier.get());
    }

    @Test
    public void testCanSuppressInAnyState() {
        assertTrue(mContent.canSuppressInAnyState());
    }
}
