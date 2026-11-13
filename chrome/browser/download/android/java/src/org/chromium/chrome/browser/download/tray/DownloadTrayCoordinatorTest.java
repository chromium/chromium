// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

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

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.RowType;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
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
        item.creationTimeMs = TimeUtils.currentTimeMillis(); // Within the model's recent window.
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

    private View shownContent() {
        View content = shownPopup().getContentView();
        assertNotNull(content);
        return content;
    }

    private SimpleRecyclerViewAdapter shownAdapter() {
        RecyclerView list = shownContent().findViewById(R.id.download_tray_list);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) list.getAdapter();
        assertNotNull(adapter);
        return adapter;
    }

    /** Builds and binds the first row as the list would; it only lays out rows when drawn. */
    private View firstRow() {
        SimpleRecyclerViewAdapter adapter = shownAdapter();
        RecyclerView list = shownContent().findViewById(R.id.download_tray_list);
        SimpleRecyclerViewAdapter.ViewHolder holder =
                adapter.createViewHolder(list, RowType.COMPLETED);
        adapter.bindViewHolder(holder, 0);
        return holder.itemView;
    }

    private static void assertHasHeaderAndFooter(View content, boolean hasHeaderAndFooter) {
        assertEquals(hasHeaderAndFooter, content.findViewById(R.id.download_tray_header) != null);
        assertEquals(
                hasHeaderAndFooter, content.findViewById(R.id.download_tray_full_history) != null);
    }

    /** Returns whether the window showing {@code content} can take focus. */
    private static boolean isFocusable(View content) {
        WindowManager.LayoutParams params =
                (WindowManager.LayoutParams) content.getRootView().getLayoutParams();
        return (params.flags & WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE) == 0;
    }

    private static void dispatchTouch(View view, int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(0, 0, action, x, y, /* metaState= */ 0);
        view.dispatchTouchEvent(event);
        event.recycle();
    }

    @Test
    public void testCompletion_showsPopupThatAutoDismisses() {
        completeDownload();
        assertEquals(1, shownAdapter().getItemCount());
        assertHasHeaderAndFooter(shownContent(), false);
        // It opens on its own, so it leaves focus in the page.
        assertFalse(isFocusable(shownContent()));

        ShadowLooper.idleMainLooper(
                DownloadTrayCoordinator.AUTO_DISMISS_MS - 1, TimeUnit.MILLISECONDS);
        shownPopup();
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testMouseEnteringPopup_cancelsAutoDismiss() {
        completeDownload();
        View row = firstRow();

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

        firstRow().performClick();

        verify(mOnActioned).run();
        verify(mDelegate).openItem(item);
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testFullTray_hasHeaderAndFooterTakesFocusAndStaysUntilClosed() {
        completeDownload();
        mCoordinator.toggleFullTray();
        View content = shownContent();
        assertEquals(1, shownAdapter().getItemCount());
        assertHasHeaderAndFooter(content, true);
        assertTrue(isFocusable(content));

        // Opened by the user, it neither auto-dismisses nor closes on hover.
        ShadowLooper.idleMainLooper(DownloadTrayCoordinator.AUTO_DISMISS_MS, TimeUnit.MILLISECONDS);
        shownPopup();

        content.findViewById(R.id.download_tray_close).performClick();
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testFullTray_closesOnOutsideTapsOnly() {
        completeDownload();
        mCoordinator.toggleFullTray();
        View header = shownContent().findViewById(R.id.download_tray_header);
        View root = header.getRootView();
        ShadowLooper.idleMainLooper();

        // A tap on a part of the tray that isn't a button, e.g. its title, leaves it open.
        int[] headerLocation = new int[2];
        header.getLocationInWindow(headerLocation);
        dispatchTouch(root, MotionEvent.ACTION_DOWN, headerLocation[0] + 1, headerLocation[1] + 1);
        shownPopup();

        dispatchTouch(root, MotionEvent.ACTION_OUTSIDE, 0, 0);
        assertNull(mCoordinator.getPopupForTesting());
    }

    @Test
    public void testFullTrayInShortWindow_onlyListShrinks() {
        completeDownload();
        mCoordinator.toggleFullTray();
        View content = shownContent();
        View header = content.findViewById(R.id.download_tray_header);
        View list = content.findViewById(R.id.download_tray_list);
        View footer = content.findViewById(R.id.download_tray_full_history);
        int widthSpec = MeasureSpec.makeMeasureSpec(320, MeasureSpec.EXACTLY);
        content.measure(widthSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        int headerHeight = header.getMeasuredHeight();
        int listHeight = list.getMeasuredHeight();
        int footerHeight = footer.getMeasuredHeight();

        // Laid out shorter than it wants, as the popup is in a short window, the header and footer
        // keep their height and the list, which scrolls, takes what is left, as on desktop.
        int shortfall = listHeight / 2;
        int shortHeight = content.getMeasuredHeight() - shortfall;
        content.measure(widthSpec, MeasureSpec.makeMeasureSpec(shortHeight, MeasureSpec.EXACTLY));
        assertEquals(headerHeight, header.getMeasuredHeight());
        assertEquals(footerHeight, footer.getMeasuredHeight());
        assertEquals(listHeight - shortfall, list.getMeasuredHeight());
    }

    @Test
    public void testFullTray_footerOpensDownloadManager() {
        completeDownload();
        mCoordinator.toggleFullTray();
        shownContent().findViewById(R.id.download_tray_full_history).performClick();

        verify(mDelegate).showDownloadManager(/* isIncognito= */ false);
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
