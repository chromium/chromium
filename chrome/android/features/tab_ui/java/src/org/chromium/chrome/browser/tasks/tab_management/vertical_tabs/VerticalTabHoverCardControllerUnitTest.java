// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;

import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabHoverCardController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabHoverCardControllerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mContainerView;
    @Mock private View mRootView;
    @Mock private View mTabView1;
    @Mock private View mTabView2;
    @Mock private ViewGroup mViewStubParent;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private VerticalTabHoverCardController mController;

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mContainerView.getRootView()).thenReturn(mRootView);
        when(mRootView.getHeight()).thenReturn(1000);
        when(mTabHoverCardView.getContext()).thenReturn(activity);
        when(mTabHoverCardViewStub.getParent()).thenReturn(mViewStubParent);

        doAnswer(
                        invocation -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            listener.onInflate(mTabHoverCardViewStub, mTabHoverCardView);
                            return null;
                        })
                .when(mTabHoverCardViewStub)
                .setOnInflateListener(any());

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getTabById(TAB_ID_2)).thenReturn(mTab2);

        mController =
                new VerticalTabHoverCardController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        /* isContextMenuShowingSupplier= */ null);
    }

    @Test
    @SmallTest
    public void testShowAndHide() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub).inflate();
        verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testSelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testDelayedShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testExitBeforeDelay_CancelsShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Hover exit before delay expires
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Running remaining delayed tasks should not trigger show()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testScrubbing_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        // Hover tab 1 and wait for delay
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());

        // Exit tab 1 hover (records exit time)
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Hover tab 2 within 300 ms buffer
        listener.onTabHoverCardStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);

        // Should show tab 2 immediately without needing ShadowLooper delay task flush
        verify(mTabHoverCardView).show(eq(mTab2), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testGetHoverCardPosition() {
        when(mTabView1.getWidth()).thenReturn(200);

        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
                        mTabView1, mContainerView, mTabHoverCardView);

        assertEquals(2, position.length);
        assertEquals(200f, position[0], 0.01f);
    }

    @Test
    @SmallTest
    public void testContextMenuShowing_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        VerticalTabHoverCardController controller =
                new VerticalTabHoverCardController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> true);

        TabHoverCardListener listener = controller.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub, never()).inflate();
    }
}
