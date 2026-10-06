// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.Rect;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ClickWithMetaStateCallback;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link HomeActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ListMenuDelegate mDelegate;
    @Mock private ListMenu mListMenu;
    @Mock private ClickWithMetaStateCallback mClickCallback;

    private Activity mActivity;
    private ListMenuButton mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new ListMenuButton(mActivity, null);
        mModel = new PropertyModel.Builder(HomeActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, HomeActionButtonBinder::bind);
    }

    @Test
    public void testLongPressMenuDelegate() {
        // The menu is only shown when the button is attached to a window.
        mActivity.setContentView(mView);
        when(mDelegate.getListMenu()).thenReturn(mListMenu);
        when(mDelegate.getRectProvider(mView)).thenReturn(new RectProvider(new Rect(0, 0, 1, 1)));
        when(mListMenu.getContentView()).thenReturn(new FrameLayout(mActivity));

        mModel.set(HomeActionProperties.LONG_PRESS_MENU_DELEGATE, mDelegate);
        // A regular click should not be overridden.
        assertFalse(mView.hasOnClickListeners());

        // The long press shows the menu provided by the delegate.
        assertTrue(mView.performLongClick());
        verify(mDelegate).getListMenu();
        assertTrue(mView.getHost().isMenuShowing());
    }

    @Test
    public void testClickWithMetaCallback() {
        mModel.set(HomeActionProperties.CLICK_WITH_META_CALLBACK, mClickCallback);
        mView.performClick();
        verify(mClickCallback).onClickWithMeta(/* metaState= */ 0, /* buttonState= */ 0);
    }

    @Test
    public void testFallbackToActionButtonBinder() {
        int resId = android.R.drawable.ic_delete;
        mModel.set(ActionProperties.ICON_ID, resId);
        assertEquals(resId, shadowOf(mView.getDrawable()).getCreatedFromResId());
    }
}
