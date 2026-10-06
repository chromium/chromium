// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

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
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

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

    private boolean shouldShow() {
        return mController.getShouldShowSupplier().get();
    }

    private void advanceTime(long millis) {
        ShadowLooper.idleMainLooper(millis, TimeUnit.MILLISECONDS);
    }

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

        // Pending auto-hide is cancelled and later events are ignored.
        advanceTime(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        added(inProgress("b"));
        assertFalse("Destroyed controller should ignore later events", shouldShow());
    }
}
