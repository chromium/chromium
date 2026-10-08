// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link DownloadToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class DownloadToolbarButtonControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OfflineContentProvider mProvider;
    @Captor private ArgumentCaptor<Callback<ArrayList<OfflineItem>>> mGetAllItemsCallbackCaptor;

    private DownloadToolbarButtonController mController;

    @Before
    public void setUp() {
        mController = new DownloadToolbarButtonController(mProvider);
        verify(mProvider).addObserver(mController);
        verify(mProvider).getAllItems(mGetAllItemsCallbackCaptor.capture());
    }

    private static ContentId id(String guid) {
        return LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, guid);
    }

    private static OfflineItem createItem(String guid, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id(guid);
        item.state = state;
        return item;
    }

    private static OfflineItem inProgress(String guid) {
        return createItem(guid, OfflineItemState.IN_PROGRESS);
    }

    private static OfflineItem inProgress(String guid, long receivedBytes, long totalBytes) {
        OfflineItem item = inProgress(guid);
        item.receivedBytes = receivedBytes;
        item.totalSizeBytes = totalBytes;
        return item;
    }

    private static OfflineItem paused(String guid) {
        return createItem(guid, OfflineItemState.PAUSED);
    }

    private static OfflineItem complete(String guid) {
        return createItem(guid, OfflineItemState.COMPLETE);
    }

    private void seed(OfflineItem... items) {
        ArrayList<OfflineItem> list = new ArrayList<>();
        Collections.addAll(list, items);
        mGetAllItemsCallbackCaptor.getValue().onResult(list);
    }

    private void added(OfflineItem item) {
        mController.onItemsAdded(List.of(item));
    }

    private void updated(OfflineItem item) {
        mController.onItemUpdated(item, /* updateDelta= */ null);
    }

    private DownloadToolbarButtonState state() {
        return mController.getStateSupplier().get();
    }

    private boolean shouldShow() {
        return state().shouldShow;
    }

    /** Asserts the published state field by field so a failure names what differed. */
    private void assertState(DownloadToolbarButtonState expected) {
        DownloadToolbarButtonState actual = state();
        assertEquals("shouldShow", expected.shouldShow, actual.shouldShow);
        assertEquals("iconState", expected.iconState, actual.iconState);
        assertEquals("isActive", expected.isActive, actual.isActive);
        assertEquals("downloadCount", expected.downloadCount, actual.downloadCount);
        assertEquals("progressPercent", expected.progressPercent, actual.progressPercent);
        assertEquals("progressCertain", expected.progressCertain, actual.progressCertain);
    }

    private void advanceTime(long millis) {
        ShadowLooper.idleMainLooper(millis, TimeUnit.MILLISECONDS);
    }

    // Visibility ---------------------------------------------------------------------------------

    @Test
    public void testSeed_noActiveItems_staysHidden() {
        assertFalse("Button should be hidden before items arrive", shouldShow());
        seed(complete("done"), createItem("cancelled", OfflineItemState.CANCELLED));
        assertFalse("Terminal items from before this session should not show", shouldShow());
    }

    @Test
    public void testSeed_withActiveItems_shows() {
        seed(complete("done"), inProgress("active"));
        assertTrue(shouldShow());
    }

    @Test
    public void testItemCompletedBeforeSeed_staleSnapshotDoesNotStrandIt() {
        // The completion arrives before the (older) snapshot that still lists the item as active.
        updated(complete("a"));
        seed(inProgress("a"));
        assertFalse("Snapshot must not resurrect an item that already completed", shouldShow());

        // Nothing is stranded as active: a later unrelated item still follows the normal
        // linger-then-hide path, which would be skipped if "a" were still counted as active.
        added(inProgress("b"));
        updated(complete("b"));
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testItemRemovedBeforeSeed_staleSnapshotDoesNotStrandIt() {
        mController.onItemRemoved(id("a"));
        seed(inProgress("a"));
        assertFalse("Snapshot must not resurrect a removed item", shouldShow());
    }

    @Test
    public void testItemCompletedBeforeSeed_activeSnapshotItemCancelsAutoHide() {
        // "a" finishing before the snapshot arrives starts the auto-hide window.
        added(inProgress("a"));
        updated(complete("a"));
        // The snapshot then reveals "b", already in flight (e.g. started in another window).
        seed(inProgress("b"));
        assertEquals(1, state().downloadCount);

        // The window must have been cancelled: once it would have elapsed, "a" is still counted
        // as ended, so removing "b" lingers rather than hiding immediately.
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        mController.onItemRemoved(id("b"));
        assertTrue("Ended item \"a\" should keep the button visible", shouldShow());
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testAllActiveStates_show() {
        seed();
        for (int state :
                List.of(
                        OfflineItemState.IN_PROGRESS,
                        OfflineItemState.PENDING,
                        OfflineItemState.PAUSED)) {
            added(createItem("a", state));
            assertTrue("State " + state + " should show the button", shouldShow());
            mController.onItemRemoved(id("a"));
            assertFalse(shouldShow());
        }
        // Both observer entry points reach the same logic.
        updated(inProgress("b"));
        assertTrue(shouldShow());
    }

    @Test
    public void testUntrackableItems_areIgnored() {
        seed();
        OfflineItem transientItem = inProgress("transient");
        transientItem.isTransient = true;
        OfflineItem suggestedItem = inProgress("suggested");
        suggestedItem.isSuggested = true;
        OfflineItem noIdItem = inProgress("x");
        noIdItem.id = null;

        added(transientItem);
        added(suggestedItem);
        added(noIdItem);
        assertFalse("Transient, suggested and id-less items should not show", shouldShow());
    }

    @Test
    public void testAllTerminalStates_lingerThenHide() {
        seed();
        // Every outcome, successful or not, keeps the button for the window, matching desktop.
        for (int state :
                List.of(
                        OfflineItemState.COMPLETE,
                        OfflineItemState.CANCELLED,
                        OfflineItemState.FAILED,
                        OfflineItemState.INTERRUPTED)) {
            added(inProgress("a"));
            updated(createItem("a", state));
            assertTrue("State " + state + " should linger", shouldShow());

            advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS - 1);
            assertTrue("State " + state + " should still show before the delay", shouldShow());
            advanceTime(1);
            assertFalse("State " + state + " should hide once the delay elapses", shouldShow());
        }
    }

    @Test
    public void testLastItemRemoved_hidesImmediately() {
        seed();
        added(inProgress("a"));
        mController.onItemRemoved(id("a"));
        assertFalse(shouldShow());
    }

    @Test
    public void testOneOfSeveralCompletes_staysVisibleWithoutTimer() {
        seed();
        added(inProgress("a"));
        added(inProgress("b"));

        updated(complete("a"));
        assertTrue(shouldShow());

        // No auto-hide should be pending while "b" is still active.
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue("Button must stay visible while another item is active", shouldShow());
    }

    @Test
    public void testNewItemDuringAutoHide_cancelsTimer() {
        seed();
        added(inProgress("a"));
        updated(complete("a"));

        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        added(inProgress("b"));

        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue("A new active item should cancel the pending auto-hide", shouldShow());
    }

    @Test
    public void testLastEndedItemRemovedDuringAutoHide_hidesImmediately() {
        seed();
        added(inProgress("a"));
        updated(complete("a"));
        assertTrue(shouldShow());

        // Deleting the only item leaves nothing for the button to surface, so it should not
        // linger for the rest of the window, matching desktop.
        mController.onItemRemoved(id("a"));
        assertFalse(shouldShow());
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testOneOfSeveralEndedItemsRemoved_keepsLingeringWithoutExtendingWindow() {
        seed();
        added(inProgress("a"));
        added(inProgress("b"));
        updated(complete("a"));
        updated(complete("b"));

        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        mController.onItemRemoved(id("a"));
        assertTrue("Another ended item remains, so the button should still linger", shouldShow());

        // Removal must not restart the window: it elapses at the originally scheduled time.
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        assertFalse(shouldShow());
    }

    @Test
    public void testLastActiveItemRemovedWhileEndedItemRemains_lingersThenHides() {
        seed();
        added(inProgress("a"));
        added(inProgress("b"));
        updated(complete("a"));

        mController.onItemRemoved(id("b"));
        assertTrue("Ended item \"a\" should keep the button visible", shouldShow());
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testEndedItemBecomesActiveAgain_isTrackedAsActive() {
        seed();
        added(inProgress("a"));
        updated(createItem("a", OfflineItemState.INTERRUPTED));
        // Retrying moves the item back to active and cancels the pending auto-hide.
        updated(inProgress("a"));
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue(shouldShow());

        // It is no longer counted as ended, so removing it hides immediately.
        mController.onItemRemoved(id("a"));
        assertFalse(shouldShow());
    }

    @Test
    public void testDestroy_unregistersResetsAndIgnoresLaterEvents() {
        seed();
        added(inProgress("a"));
        updated(complete("a"));
        assertTrue(shouldShow());

        // Destroying mid auto-hide must not leave retained observers with a stale "show".
        mController.destroy();
        verify(mProvider).removeObserver(mController);
        assertFalse("Destroy should reset the supplier", shouldShow());
        assertState(DownloadToolbarButtonState.HIDDEN);

        // Pending auto-hide is cancelled and later events are ignored.
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        added(inProgress("b"));
        assertFalse("Destroyed controller should ignore later events", shouldShow());
    }

    @Test
    public void testDestroy_whileDownloadActive_publishesHidden() {
        seed();
        added(inProgress("active", 50, 100));
        added(inProgress("ended"));
        updated(complete("ended"));
        assertTrue(state().isActive);
        assertEquals(1, state().downloadCount);

        // Tracked items are dropped on destroy, so the published state must not keep reporting
        // the in-flight download.
        mController.destroy();
        assertState(DownloadToolbarButtonState.HIDDEN);

        updated(inProgress("active", 60, 100));
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    // Published state ----------------------------------------------------------------------------

    @Test
    public void testState_singleDownload_isProgressAndActive() {
        assertState(DownloadToolbarButtonState.HIDDEN);
        seed();
        assertState(DownloadToolbarButtonState.HIDDEN);

        added(inProgress("a", 25, 100));
        assertState(
                new DownloadToolbarButtonState(
                        /* shouldShow= */ true,
                        IconState.PROGRESS,
                        /* isActive= */ true,
                        /* downloadCount= */ 1,
                        /* progressPercent= */ 25,
                        /* progressCertain= */ true));
    }

    @Test
    public void testState_progressAggregatesAcrossDownloads() {
        seed();
        added(inProgress("a", 25, 100));
        added(inProgress("b", 75, 100));
        assertEquals(2, state().downloadCount);
        assertEquals("(25 + 75) / (100 + 100)", 50, state().progressPercent);
        assertTrue(state().progressCertain);

        // Progress updates for an existing item replace its contribution rather than adding to it.
        updated(inProgress("a", 100, 100));
        assertEquals(2, state().downloadCount);
        assertEquals("(100 + 75) / 200, floored", 87, state().progressPercent);

        // Totals and byte counts outside the expected range must not push the aggregate outside
        // [0, 100].
        updated(inProgress("b", 300, 100));
        assertEquals(100, state().progressPercent);
        updated(inProgress("b", -300, 100));
        assertEquals(0, state().progressPercent);
    }

    @Test
    public void testState_unknownTotalMakesProgressUncertain() {
        seed();
        added(inProgress("a", 10, 0));
        assertEquals("No known totals: nothing to report", 0, state().progressPercent);
        assertFalse(state().progressCertain);

        // An item of unknown size is counted but excluded from the percentage, as on desktop.
        added(inProgress("b", 50, 100));
        assertEquals(2, state().downloadCount);
        assertFalse(state().progressCertain);
        assertEquals("Only the item with a known total contributes", 50, state().progressPercent);
    }

    @Test
    public void testState_allPaused_isInactive() {
        seed();
        added(paused("a"));
        assertEquals(IconState.PROGRESS, state().iconState);
        assertFalse("A lone paused download should render inactive", state().isActive);
        assertTrue(shouldShow());

        // Any running download alongside the paused one makes the icon active again.
        added(inProgress("b"));
        assertTrue(state().isActive);
        assertEquals(2, state().downloadCount);

        updated(paused("b"));
        assertFalse("All downloads paused should render inactive", state().isActive);

        updated(inProgress("a"));
        assertTrue("Resuming should render active", state().isActive);
    }

    @Test
    public void testState_lingeringAfterCompletion_isCompleteAndInactive() {
        seed();
        added(inProgress("a", 100, 100));
        updated(complete("a"));
        assertState(
                new DownloadToolbarButtonState(
                        /* shouldShow= */ true,
                        IconState.COMPLETE,
                        /* isActive= */ false,
                        /* downloadCount= */ 0,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ true));

        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    @Test
    public void testState_oneOfSeveralLeaves_staysInProgressWithRemainingItems() {
        seed();
        added(inProgress("a", 0, 100));
        added(inProgress("b", 50, 100));
        added(inProgress("c", 100, 100));
        assertEquals(3, state().downloadCount);
        assertEquals(50, state().progressPercent);

        // Whether an item completes or is removed, only the remaining active items count.
        updated(complete("a"));
        assertEquals(IconState.PROGRESS, state().iconState);
        assertEquals(2, state().downloadCount);
        assertEquals(75, state().progressPercent);

        mController.onItemRemoved(id("b"));
        assertEquals(1, state().downloadCount);
        assertEquals(100, state().progressPercent);
    }

    @Test
    public void testState_notRepublishedWhenUnchanged() {
        seed();
        AtomicInteger notifications = new AtomicInteger();
        mController.getStateSupplier().addSyncObserver(state -> notifications.incrementAndGet());

        added(inProgress("a", 10, 100));
        assertEquals(1, notifications.get());

        // Same bytes again: nothing observable changed, so observers must not be re-notified.
        updated(inProgress("a", 10, 100));
        assertEquals(1, notifications.get());

        // A terminal update for an item never seen active changes nothing either.
        updated(complete("unrelated"));
        assertEquals(1, notifications.get());

        updated(inProgress("a", 20, 100));
        assertEquals(2, notifications.get());
    }
}
