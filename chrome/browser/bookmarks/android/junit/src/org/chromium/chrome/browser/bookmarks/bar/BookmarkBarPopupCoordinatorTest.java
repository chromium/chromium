// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;
import android.widget.PopupWindow;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit tests for the {@link BookmarkBarPopupCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarPopupCoordinatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mBookmarkBarView;
    @Mock private View mAnchorView;
    @Mock private ChromePopupWindow mMockPopupWindow;
    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;

    private Activity mActivity;
    private BookmarkBarPopupCoordinator mCoordinator;
    private UiWidgetFactory mOriginalUiWidgetFactory;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        when(mMockPopupWindow.getBackground()).thenReturn(new ColorDrawable(Color.TRANSPARENT));

        mCoordinator =
                new BookmarkBarPopupCoordinator(
                        mActivity,
                        mBookmarkBarView,
                        () -> new Pair<>(0, 0)); // controlsHeightSupplier

        mOriginalUiWidgetFactory = UiWidgetFactory.getInstance();
        UiWidgetFactory.setInstance(
                new UiWidgetFactory() {
                    @Override
                    public ChromePopupWindow createPopupWindow(Context context) {
                        return mMockPopupWindow;
                    }
                });
    }

    @After
    public void tearDown() {
        UiWidgetFactory.setInstance(mOriginalUiWidgetFactory);
    }

    @Test
    @SmallTest
    public void testShowFolderItemsPopup_usesTransparentBackground() {
        View rootView = new View(mActivity);
        when(mBookmarkBarView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        mCoordinator.showFolderItemsPopup(mAnchorView, new ModelList(), /* isIncognito= */ false);

        verify(mMockPopupWindow).setBackgroundDrawable(mDrawableCaptor.capture());

        assertTrue(mDrawableCaptor.getValue() instanceof ColorDrawable);
        assertEquals(Color.TRANSPARENT, ((ColorDrawable) mDrawableCaptor.getValue()).getColor());
    }

    @Test
    @SmallTest
    public void testShowFolderItemsPopup_setsSelectedState() {
        View rootView = new View(mActivity);
        when(mBookmarkBarView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        mCoordinator.showFolderItemsPopup(mAnchorView, new ModelList(), /* isIncognito= */ false);

        // Verify anchorView is selected when popup is shown.
        verify(mAnchorView).setSelected(true);

        ArgumentCaptor<PopupWindow.OnDismissListener> dismissListenerCaptor =
                ArgumentCaptor.forClass(PopupWindow.OnDismissListener.class);

        // Verify that the dismiss listener is registered on the popup window.
        verify(mMockPopupWindow).setOnDismissListener(dismissListenerCaptor.capture());

        // Trigger the dismiss listener.
        dismissListenerCaptor.getValue().onDismiss();

        // Verify anchorView is deselected when popup is dismissed.
        verify(mAnchorView).setSelected(false);
    }

    @Test
    @SmallTest
    public void testDismiss_dismissesBothPopups() {
        AnchoredPopupWindow folderPopup = mock(AnchoredPopupWindow.class);
        AnchoredPopupWindow contextMenuPopup = mock(AnchoredPopupWindow.class);
        mCoordinator.mFolderPopup.setPopupWindowForTesting(folderPopup);
        mCoordinator.mContextMenuPopup.setPopupWindowForTesting(contextMenuPopup);

        mCoordinator.dismiss();
        verify(folderPopup).dismiss();
        verify(contextMenuPopup).dismiss();
    }

    @Test
    @SmallTest
    public void testShowContextMenuPopup_setsSelectedStateOnSubitem() {
        View rootView = new View(mActivity);
        when(mBookmarkBarView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getRootView()).thenReturn(rootView);
        when(mAnchorView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        View subitemView = mock(View.class);
        when(subitemView.getRootView()).thenReturn(rootView);
        when(subitemView.getViewTreeObserver()).thenReturn(rootView.getViewTreeObserver());

        mCoordinator.showFolderItemsPopup(mAnchorView, new ModelList(), /* isIncognito= */ false);

        mCoordinator.showContextMenuPopup(
                new ModelList(), subitemView, /* offset= */ null, /* isIncognito= */ false);

        verify(subitemView).setSelected(true);

        ArgumentCaptor<PopupWindow.OnDismissListener> dismissListenerCaptor =
                ArgumentCaptor.forClass(PopupWindow.OnDismissListener.class);

        verify(mMockPopupWindow, atLeastOnce())
                .setOnDismissListener(dismissListenerCaptor.capture());

        // Trigger the dismiss listener for context menu.
        dismissListenerCaptor.getValue().onDismiss();

        verify(subitemView).setSelected(false);
    }
}
