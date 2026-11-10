// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link DownloadTrayCoordinator}'s popup lifecycle. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class DownloadTrayCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OfflineContentProvider mProvider;
    @Mock private DownloadTrayDelegate mDelegate;
    @Mock private Runnable mOnActioned;

    private DownloadStateModel mModel;
    private DownloadTrayCoordinator mCoordinator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout root = new FrameLayout(activity);
        View anchor = new View(activity);
        root.addView(anchor, new FrameLayout.LayoutParams(100, 100));
        activity.setContentView(root);
        ShadowLooper.idleMainLooper();

        mModel = new DownloadStateModel(mProvider);
        mCoordinator =
                new DownloadTrayCoordinator(
                        activity,
                        mModel,
                        () -> anchor,
                        /* isWindowFocused= */ () -> true,
                        /* isIncognitoSelected= */ () -> false,
                        mOnActioned,
                        mDelegate);
    }

    @After
    public void tearDown() {
        // Dismisses any popup still showing; safe to call again after testDestroy_dismissesPopup.
        mCoordinator.destroy();
        AccessibilityStateTestHelper.uninitializeForTesting();
    }

    private OfflineItem completeDownload() {
        OfflineItem item = new OfflineItem();
        item.id = LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, "a");
        item.title = "report.pdf";
        item.state = OfflineItemState.IN_PROGRESS;
        mModel.onItemsAdded(List.of(item));
        OfflineItem completed = item.clone();
        completed.state = OfflineItemState.COMPLETE;
        mModel.onItemUpdated(completed, /* updateDelta= */ null);
        return completed;
    }

    private AnchoredPopupWindow shownPopup() {
        AnchoredPopupWindow popup = mCoordinator.getPopupForTesting();
        assertNotNull(popup);
        assertTrue(popup.isShowing());
        return popup;
    }

    @Test
    public void testCompletion_showsPopupThatAutoDismisses() {
        completeDownload();
        shownPopup();

        ShadowLooper.idleMainLooper(
                DownloadTrayCoordinator.AUTO_DISMISS_MS - 1, TimeUnit.MILLISECONDS);
        shownPopup();
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testMouseEnteringPopup_cancelsAutoDismiss() {
        completeDownload();
        View row = shownPopup().getContentView();
        assertNotNull(row);

        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0, 0, /* metaState= */ 0);
        hoverEnter.setSource(InputDevice.SOURCE_MOUSE);
        row.dispatchGenericMotionEvent(hoverEnter);
        hoverEnter.recycle();

        ShadowLooper.idleMainLooper(DownloadTrayCoordinator.AUTO_DISMISS_MS, TimeUnit.MILLISECONDS);
        shownPopup();
    }

    @Test
    public void testScreenReader_popupDoesNotAutoDismiss() {
        AccessibilityStateTestHelper.setIsTouchExplorationEnabledForTesting(true);
        completeDownload();

        ShadowLooper.idleMainLooper(DownloadTrayCoordinator.AUTO_DISMISS_MS, TimeUnit.MILLISECONDS);
        shownPopup();
    }

    @Test
    public void testClickingRow_opensAndDismisses() {
        OfflineItem item = completeDownload();

        View row = shownPopup().getContentView();
        assertNotNull(row);
        row.performClick();

        verify(mOnActioned).run();
        verify(mDelegate).openItem(item);
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testDestroy_dismissesPopup() {
        completeDownload();
        shownPopup();

        mCoordinator.destroy();
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    @Config(qualifiers = "w320dp-h40dp")
    public void testNoRoomToShow_resetsState() {
        // Too short a window for a touchable popup, so AnchoredPopupWindow doesn't show it.
        completeDownload();
        assertNull(mCoordinator.getPopupForTesting());
    }
}
