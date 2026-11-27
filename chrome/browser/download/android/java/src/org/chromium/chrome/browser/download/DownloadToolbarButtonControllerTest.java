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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.download.DownloadToolbarButtonMetrics.ButtonState;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
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
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Captor private ArgumentCaptor<Callback<ArrayList<OfflineItem>>> mGetAllItemsCallbackCaptor;
    @Captor private ArgumentCaptor<IncognitoStateObserver> mIncognitoStateObserverCaptor;

    private DownloadStateModel mModel;
    private DownloadToolbarButtonController mController;

    @Before
    public void setUp() {
        // A real model over a mocked provider keeps these tests end to end.
        mModel = new DownloadStateModel(mProvider);
        verify(mProvider).getAllItems(mGetAllItemsCallbackCaptor.capture());
        mController = new DownloadToolbarButtonController(mModel, mIncognitoStateProvider);
        verify(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(mIncognitoStateObserverCaptor.capture());
    }

    /** Reports the window switching between regular and incognito tabs. */
    private void setIncognitoSelected(boolean isIncognitoSelected) {
        mIncognitoStateObserverCaptor.getValue().onIncognitoStateChanged(isIncognitoSelected);
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

    private static OfflineItem createInProgressItem(String guid) {
        return createItem(guid, OfflineItemState.IN_PROGRESS);
    }

    private static OfflineItem createInProgressItem(
            String guid, long receivedBytes, long totalBytes) {
        OfflineItem item = createInProgressItem(guid);
        item.receivedBytes = receivedBytes;
        item.totalSizeBytes = totalBytes;
        return item;
    }

    private static OfflineItem createPausedItem(String guid) {
        return createItem(guid, OfflineItemState.PAUSED);
    }

    private static OfflineItem createCompletedItem(String guid) {
        return createItem(guid, OfflineItemState.COMPLETE);
    }

    private static OfflineItem createIncognitoItem(
            String guid, @OfflineItemState int state, OtrProfileId profileId) {
        OfflineItem item = createItem(guid, state);
        item.isOffTheRecord = true;
        item.otrProfileId = OtrProfileId.serialize(profileId);
        return item;
    }

    private void onAllItemsRetrieved(OfflineItem... items) {
        ArrayList<OfflineItem> list = new ArrayList<>();
        Collections.addAll(list, items);
        mGetAllItemsCallbackCaptor.getValue().onResult(list);
    }

    private void onItemAdded(OfflineItem item) {
        mModel.onItemsAdded(List.of(item));
    }

    private void onItemUpdated(OfflineItem item) {
        mModel.onItemUpdated(item, /* updateDelta= */ null);
    }

    private void onItemRemoved(String guid) {
        mModel.onItemRemoved(id(guid));
    }

    private DownloadToolbarButtonState state() {
        return mController.getStateSupplier().get();
    }

    private boolean shouldShow() {
        return state().shouldShow;
    }

    /** Asserts the button is shown lingering after completions, i.e. with the complete icon. */
    private void assertLingering() {
        assertTrue(shouldShow());
        assertEquals(IconState.COMPLETE, state().iconState);
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

    private void advanceTimeMs(long millis) {
        ShadowLooper.idleMainLooper(millis, TimeUnit.MILLISECONDS);
    }

    // Visibility ---------------------------------------------------------------------------------

    @Test
    public void testAttachedAfterSeed_showsModelStateImmediately() {
        // E.g. a provider that delivers its snapshot synchronously, before the controller exists.
        onAllItemsRetrieved(createInProgressItem("a", 40, 100));
        DownloadToolbarButtonController controller =
                new DownloadToolbarButtonController(mModel, mIncognitoStateProvider);
        assertTrue(controller.getStateSupplier().get().shouldShow);
        assertEquals(40, controller.getStateSupplier().get().progressPercent);
        controller.destroy();
    }

    @Test
    public void testItemCompletedBeforeSeed_activeSnapshotItemCancelsAutoHide() {
        // "a" finishing before the snapshot arrives starts the auto-hide window.
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createCompletedItem("a"));
        // The snapshot then reveals "b", already in flight (e.g. started in another window).
        onAllItemsRetrieved(createInProgressItem("b"));
        assertEquals(1, state().downloadCount);

        // The window must have been cancelled: once it would have elapsed, "a" is still counted
        // as ended, so removing "b" lingers rather than hiding immediately.
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        onItemRemoved("b");
        assertTrue("Ended item \"a\" should keep the button visible", shouldShow());
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testListedTerminalStates_lingerThenHide() {
        onAllItemsRetrieved();
        // Completed and interrupted downloads, which the download UI lists, keep the button for
        // the window, as on desktop. Only a successful completion draws attention.
        for (int state : List.of(OfflineItemState.COMPLETE, OfflineItemState.INTERRUPTED)) {
            onItemAdded(createInProgressItem("a"));
            onItemUpdated(createItem("a", state));
            assertTrue("State " + state + " should linger", shouldShow());
            assertEquals(IconState.COMPLETE, state().iconState);
            assertEquals(
                    "Only COMPLETE should be active",
                    state == OfflineItemState.COMPLETE,
                    state().isActive);

            advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS - 1);
            assertTrue("State " + state + " should still show before the delay", shouldShow());
            advanceTimeMs(1);
            assertFalse("State " + state + " should hide once the delay elapses", shouldShow());
        }
    }

    @Test
    public void testUnlistedTerminalStates_hideImmediately() {
        onAllItemsRetrieved();
        // As on the Downloads page, cancelled and failed downloads aren't listed, so the button
        // doesn't linger to surface them.
        for (int state : List.of(OfflineItemState.CANCELLED, OfflineItemState.FAILED)) {
            onItemAdded(createInProgressItem("a"));
            onItemUpdated(createItem("a", state));
            assertFalse("State " + state + " should not linger", shouldShow());
        }
    }

    @Test
    public void testInterruptedItemCancelled_stopsLingering() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createItem("a", OfflineItemState.INTERRUPTED));
        assertTrue(shouldShow());

        // E.g. cancelled from the Downloads page, so no longer listed.
        onItemUpdated(createItem("a", OfflineItemState.CANCELLED));
        assertFalse(shouldShow());
    }

    @Test
    public void testLastItemRemoved_hidesImmediately() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemRemoved("a");
        assertFalse(shouldShow());
    }

    @Test
    public void testOneOfSeveralCompletes_staysVisibleWithoutTimer() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemAdded(createInProgressItem("b"));

        onItemUpdated(createCompletedItem("a"));
        assertTrue(shouldShow());

        // No auto-hide should be pending while "b" is still active.
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue("Button must stay visible while another item is active", shouldShow());
    }

    @Test
    public void testNewItemDuringAutoHide_cancelsTimer() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createCompletedItem("a"));

        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        onItemAdded(createInProgressItem("b"));

        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue("A new active item should cancel the pending auto-hide", shouldShow());
    }

    @Test
    public void testLastEndedItemRemovedDuringAutoHide_hidesImmediately() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createCompletedItem("a"));
        assertTrue(shouldShow());

        // Deleting the only item leaves nothing for the button to surface, so it should not
        // linger for the rest of the window, matching desktop.
        onItemRemoved("a");
        assertFalse(shouldShow());
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testOneOfSeveralEndedItemsRemoved_keepsLingeringWithoutExtendingWindow() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemAdded(createInProgressItem("b"));
        onItemUpdated(createCompletedItem("a"));
        onItemUpdated(createCompletedItem("b"));

        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        onItemRemoved("a");
        assertTrue("Another ended item remains, so the button should still linger", shouldShow());

        // Removal must not restart the window: it elapses at the originally scheduled time.
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        assertFalse(shouldShow());
    }

    @Test
    public void testLastActiveItemRemovedWhileEndedItemRemains_lingersThenHides() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemAdded(createInProgressItem("b"));
        onItemUpdated(createCompletedItem("a"));

        onItemRemoved("b");
        assertTrue("Ended item \"a\" should keep the button visible", shouldShow());
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertFalse(shouldShow());
    }

    @Test
    public void testEndedItemBecomesActiveAgain_isTrackedAsActive() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createItem("a", OfflineItemState.INTERRUPTED));
        // Retrying moves the item back to active and cancels the pending auto-hide.
        onItemUpdated(createInProgressItem("a"));
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertTrue(shouldShow());

        // It is no longer counted as ended, so removing it hides immediately.
        onItemRemoved("a");
        assertFalse(shouldShow());
    }

    @Test
    public void testDestroy_publishesHiddenAndIgnoresLaterEvents() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("active", 50, 100));
        onItemAdded(createInProgressItem("ended"));
        onItemUpdated(createCompletedItem("ended"));
        assertTrue(shouldShow());

        // The model outlives the controller and still holds "active", but retained observers of
        // the supplier must not keep a stale state.
        mController.destroy();
        assertState(DownloadToolbarButtonState.HIDDEN);
        verify(mIncognitoStateProvider).removeObserver(mIncognitoStateObserverCaptor.getValue());

        // Later timers and model events are ignored.
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        onItemUpdated(createInProgressItem("active", 60, 100));
        onItemAdded(createInProgressItem("b"));
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    // Published state ----------------------------------------------------------------------------

    @Test
    public void testState_singleDownload_isProgressAndActive() {
        assertState(DownloadToolbarButtonState.HIDDEN);
        onAllItemsRetrieved();
        assertState(DownloadToolbarButtonState.HIDDEN);

        onItemAdded(createInProgressItem("a", 25, 100));
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
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a", 25, 100));
        onItemAdded(createInProgressItem("b", 75, 100));
        assertEquals(2, state().downloadCount);
        assertEquals("(25 + 75) / (100 + 100)", 50, state().progressPercent);
        assertTrue(state().progressCertain);

        // Progress updates for an existing item replace its contribution rather than adding to it.
        onItemUpdated(createInProgressItem("a", 100, 100));
        assertEquals(2, state().downloadCount);
        assertEquals("(100 + 75) / 200, floored", 87, state().progressPercent);

        // Totals and byte counts outside the expected range must not push the aggregate outside
        // [0, 100].
        onItemUpdated(createInProgressItem("b", 300, 100));
        assertEquals(100, state().progressPercent);
        onItemUpdated(createInProgressItem("b", -300, 100));
        assertEquals(0, state().progressPercent);
    }

    @Test
    public void testState_unknownTotalMakesProgressUncertain() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a", 10, 0));
        assertEquals("No known totals: nothing to report", 0, state().progressPercent);
        assertFalse(state().progressCertain);

        // An item of unknown size is counted but excluded from the percentage, as on desktop.
        onItemAdded(createInProgressItem("b", 50, 100));
        assertEquals(2, state().downloadCount);
        assertFalse(state().progressCertain);
        assertEquals("Only the item with a known total contributes", 50, state().progressPercent);
    }

    @Test
    public void testState_allPaused_isInactive() {
        onAllItemsRetrieved();
        onItemAdded(createPausedItem("a"));
        assertEquals(IconState.PROGRESS, state().iconState);
        assertFalse("A lone paused download should render inactive", state().isActive);
        assertTrue(shouldShow());

        // Any running download alongside the paused one makes the icon active again.
        onItemAdded(createInProgressItem("b"));
        assertTrue(state().isActive);
        assertEquals(2, state().downloadCount);

        onItemUpdated(createPausedItem("b"));
        assertFalse("All downloads paused should render inactive", state().isActive);

        onItemUpdated(createInProgressItem("a"));
        assertTrue("Resuming should render active", state().isActive);
    }

    @Test
    public void testState_lingeringAfterCompletion_isActiveForWindowThenInactive() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a", 100, 100));
        onItemUpdated(createCompletedItem("a"));
        assertState(
                new DownloadToolbarButtonState(
                        /* shouldShow= */ true,
                        IconState.COMPLETE,
                        /* isActive= */ true,
                        /* downloadCount= */ 0,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ true));

        // The active colour lasts for the window, then the button lingers inactive.
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS - 1);
        assertTrue(state().isActive);
        advanceTimeMs(1);
        assertFalse(state().isActive);
        assertEquals(IconState.COMPLETE, state().iconState);
        assertTrue(shouldShow());

        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    @Test
    public void testState_removedCompletion_noLongerDrawsAttention() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemAdded(createInProgressItem("b"));
        onItemUpdated(createCompletedItem("a"));
        onItemUpdated(createCompletedItem("b"));
        onItemRemoved("a");
        assertTrue("\"b\" still completed recently", state().isActive);

        // Once the last recent completion is gone, a subsequent interruption within the window
        // must not inherit its accent.
        onItemRemoved("b");
        onItemAdded(createInProgressItem("c"));
        onItemUpdated(createItem("c", OfflineItemState.INTERRUPTED));
        assertEquals(IconState.COMPLETE, state().iconState);
        assertFalse(state().isActive);
    }

    @Test
    public void testMarkActioned_endsActiveWindowImmediately() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createCompletedItem("a"));
        assertTrue(state().isActive);

        mController.markActioned();
        assertFalse(state().isActive);
        assertTrue("Acting on the button does not hide it", shouldShow());

        // The cancelled window must not flip anything later, and a redundant markActioned() is a
        // no-op rather than a republish.
        AtomicInteger notifications = new AtomicInteger();
        mController.getStateSupplier().addSyncObserver(state -> notifications.incrementAndGet());
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS);
        mController.markActioned();
        assertEquals(0, notifications.get());
    }

    @Test
    public void testState_newCompletionRestartsActiveWindow() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemUpdated(createCompletedItem("a"));
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS / 2);

        onItemAdded(createInProgressItem("b"));
        onItemUpdated(createCompletedItem("b"));
        // Past the end of "a"'s window, but "b" restarted it.
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS / 2 + 1);
        assertTrue(state().isActive);
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS / 2);
        assertFalse(state().isActive);
    }

    @Test
    public void testState_completionWhileOthersActive_doesNotAffectProgressActiveness() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a"));
        onItemAdded(createPausedItem("b"));
        onItemUpdated(createCompletedItem("a"));
        // While anything is still downloading the progress rules decide the colour, so an
        // all-paused remainder is inactive despite the recent completion.
        assertEquals(IconState.PROGRESS, state().iconState);
        assertFalse(state().isActive);

        // Once the remainder ends within the window the completion accent applies.
        onItemUpdated(createCompletedItem("b"));
        assertEquals(IconState.COMPLETE, state().iconState);
        assertTrue(state().isActive);
    }

    @Test
    public void testState_oneOfSeveralLeaves_staysInProgressWithRemainingItems() {
        onAllItemsRetrieved();
        onItemAdded(createInProgressItem("a", 0, 100));
        onItemAdded(createInProgressItem("b", 50, 100));
        onItemAdded(createInProgressItem("c", 100, 100));
        assertEquals(3, state().downloadCount);
        assertEquals(50, state().progressPercent);

        // Whether an item completes or is removed, only the remaining active items count.
        onItemUpdated(createCompletedItem("a"));
        assertEquals(IconState.PROGRESS, state().iconState);
        assertEquals(2, state().downloadCount);
        assertEquals(75, state().progressPercent);

        onItemRemoved("b");
        assertEquals(1, state().downloadCount);
        assertEquals(100, state().progressPercent);
    }

    @Test
    public void testState_notRepublishedWhenUnchanged() {
        onAllItemsRetrieved();
        AtomicInteger notifications = new AtomicInteger();
        mController.getStateSupplier().addSyncObserver(state -> notifications.incrementAndGet());

        onItemAdded(createInProgressItem("a", 10, 100));
        assertEquals(1, notifications.get());

        // Same bytes again: nothing observable changed, so observers must not be re-notified.
        onItemUpdated(createInProgressItem("a", 10, 100));
        assertEquals(1, notifications.get());

        // Terminal updates and removals for items never seen active change nothing either.
        onItemUpdated(createCompletedItem("unrelated"));
        onItemRemoved("unrelated");
        assertEquals(1, notifications.get());

        onItemUpdated(createInProgressItem("a", 20, 100));
        assertEquals(2, notifications.get());
    }

    // Incognito -----------------------------------------------------------------------------------

    @Test
    public void testIncognitoDownloads_onlyCountWhileIncognitoTabsAreShown() {
        onAllItemsRetrieved();
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        onItemAdded(createIncognitoItem("incognito", OfflineItemState.IN_PROGRESS, primary));
        // Incognito Custom Tabs use their own profile, which incognito tabs don't show either.
        onItemAdded(
                createIncognitoItem(
                        "cct", OfflineItemState.IN_PROGRESS, new OtrProfileId("CCT:Incognito-1")));
        // Showing regular tabs, incognito downloads don't surface at all.
        assertState(DownloadToolbarButtonState.HIDDEN);

        // Showing incognito tabs, the window's own incognito downloads count, with regular ones.
        setIncognitoSelected(true);
        assertEquals(1, state().downloadCount);
        onItemAdded(createInProgressItem("regular"));
        assertEquals(2, state().downloadCount);

        // Back on regular tabs, only the regular download counts.
        setIncognitoSelected(false);
        assertEquals(1, state().downloadCount);
    }

    @Test
    public void testIncognitoCompletion_isTrackedButOnlySurfacedWhileIncognitoTabsAreShown() {
        onAllItemsRetrieved();
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        onItemAdded(createIncognitoItem("a", OfflineItemState.IN_PROGRESS, primary));
        onItemUpdated(createIncognitoItem("a", OfflineItemState.COMPLETE, primary));
        assertState(DownloadToolbarButtonState.HIDDEN);

        // Switching to incognito tabs within the active window shows the completion as such.
        setIncognitoSelected(true);
        assertState(
                new DownloadToolbarButtonState(
                        /* shouldShow= */ true,
                        IconState.COMPLETE,
                        /* isActive= */ true,
                        /* downloadCount= */ 0,
                        /* progressPercent= */ 0,
                        /* progressCertain= */ true));

        // The active-colour window follows the download, not the window: it elapses even while
        // regular tabs are shown.
        setIncognitoSelected(false);
        advanceTimeMs(DownloadToolbarButtonController.ACTIVE_AFTER_COMPLETE_MS);
        setIncognitoSelected(true);
        assertFalse(state().isActive);
        // The auto-hide window, by contrast, only runs while this window shows the ended item
        // (see testAutoHide_followsTheItemsTheWindowShows).
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    @Test
    public void testAutoHide_followsTheItemsTheWindowShows() {
        onAllItemsRetrieved();
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        onItemAdded(createInProgressItem("regular"));
        onItemAdded(createIncognitoItem("incognito", OfflineItemState.IN_PROGRESS, primary));

        // Both complete while regular tabs are shown. The window shows only the regular one
        // lingering, and hides it on time even though the incognito completion is also waiting.
        onItemUpdated(createIncognitoItem("incognito", OfflineItemState.COMPLETE, primary));
        assertEquals(IconState.PROGRESS, state().iconState);
        onItemUpdated(createCompletedItem("regular"));
        assertLingering();
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertState(DownloadToolbarButtonState.HIDDEN);

        // The incognito completion kept its own window, which only runs while incognito tabs are
        // shown: switching away stops it and switching back starts it afresh.
        setIncognitoSelected(true);
        assertLingering();
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS / 2);
        setIncognitoSelected(false);
        assertState(DownloadToolbarButtonState.HIDDEN);
        setIncognitoSelected(true);
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS - 1);
        assertLingering();
        advanceTimeMs(1);
        assertState(DownloadToolbarButtonState.HIDDEN);
    }

    // Metrics ------------------------------------------------------------------------------------

    @Test
    public void testMetrics_recordsClickedByButtonState() {
        onAllItemsRetrieved();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DownloadToolbarButtonMetrics.CLICKED_HISTOGRAM,
                                ButtonState.IN_PROGRESS,
                                ButtonState.IN_PROGRESS,
                                ButtonState.COMPLETE_ACTIVE,
                                ButtonState.COMPLETE_INACTIVE,
                                ButtonState.COMPLETE_INACTIVE)
                        .build();

        // Ignored while hidden.
        mController.recordButtonClicked();

        onItemAdded(createInProgressItem("a", 10, 100));
        mController.recordButtonClicked();

        // All-paused still uses the progress icon state.
        onItemUpdated(createPausedItem("a"));
        mController.recordButtonClicked();

        onItemUpdated(createCompletedItem("a"));
        mController.recordButtonClicked();

        mController.markActioned();
        mController.recordButtonClicked();

        // Lingering on an interrupted download is CompleteInactive without markActioned().
        onItemRemoved("a");
        onItemAdded(createInProgressItem("b"));
        onItemUpdated(createItem("b", OfflineItemState.INTERRUPTED));
        mController.recordButtonClicked();

        // Ignored after destroy.
        mController.destroy();
        mController.recordButtonClicked();

        watcher.assertExpected();
    }

    @Test
    public void testMetrics_recordsShownOncePerShowCycle() {
        onAllItemsRetrieved();

        // Hidden -> shown records once; progress ticks and completion do not re-record.
        HistogramWatcher firstShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("a", 10, 100));
        onItemUpdated(createInProgressItem("a", 50, 100));
        onItemUpdated(createCompletedItem("a"));
        firstShow.assertExpected();

        // After auto-hide, a new download starts a fresh show cycle.
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        HistogramWatcher secondShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("b"));
        secondShow.assertExpected();
    }

    @Test
    public void testMetrics_shownAcrossIncognitoSwitches() {
        onAllItemsRetrieved();
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();

        // An incognito download started on regular tabs stays hidden until switching to incognito.
        HistogramWatcher noRecord =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM)
                        .build();
        onItemAdded(createIncognitoItem("incog", OfflineItemState.IN_PROGRESS, primary));
        noRecord.assertExpected();

        // First switch to incognito records once; round-tripping through regular (even with a
        // progress tick arriving while on regular) does not double-count.
        HistogramWatcher incognitoShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        setIncognitoSelected(true);
        setIncognitoSelected(false);
        OfflineItem tick = createIncognitoItem("incog", OfflineItemState.IN_PROGRESS, primary);
        tick.receivedBytes = 50;
        tick.totalSizeBytes = 100;
        onItemUpdated(tick);
        setIncognitoSelected(true);
        incognitoShow.assertExpected();

        // Once all downloads are gone, a new regular download starts a fresh show cycle — even if
        // a non-primary OTR download (never visible in this window) ended in this session.
        OtrProfileId nonPrimary = new OtrProfileId("CCT:Incognito-1");
        onItemAdded(createIncognitoItem("cct", OfflineItemState.IN_PROGRESS, nonPrimary));
        onItemUpdated(createIncognitoItem("cct", OfflineItemState.COMPLETE, nonPrimary));
        setIncognitoSelected(false);
        onItemRemoved("incog");
        HistogramWatcher regularShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("regular"));
        setIncognitoSelected(true);
        regularShow.assertExpected();
    }

    @Test
    public void testMetrics_endedIncognitoItemDoesNotSuppressLaterRegularShowCycles() {
        onAllItemsRetrieved();
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();

        // Both a regular and an incognito download complete while on regular tabs. Regular
        // auto-hide clears only the regular item, leaving the incognito completion pending.
        HistogramWatcher firstRegularShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("regular1"));
        onItemAdded(createIncognitoItem("incog", OfflineItemState.IN_PROGRESS, primary));
        onItemUpdated(createIncognitoItem("incog", OfflineItemState.COMPLETE, primary));
        onItemUpdated(createCompletedItem("regular1"));
        firstRegularShow.assertExpected();
        advanceTimeMs(DownloadToolbarButtonController.AUTO_HIDE_DELAY_MS);
        assertState(DownloadToolbarButtonState.HIDDEN);

        // A later regular download starts a fresh show cycle despite the pending incognito
        // completion; cancelling or removing it also ends the cycle without auto-hide.
        HistogramWatcher secondRegularShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("regular2"));
        secondRegularShow.assertExpected();
        onItemUpdated(createItem("regular2", OfflineItemState.CANCELLED));
        assertState(DownloadToolbarButtonState.HIDDEN);

        HistogramWatcher thirdRegularShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        onItemAdded(createInProgressItem("regular3"));
        thirdRegularShow.assertExpected();
        onItemRemoved("regular3");
        assertState(DownloadToolbarButtonState.HIDDEN);

        // Switching to incognito surfaces the lingering incognito completion once; round-tripping
        // through regular tabs while it lingers does not double-count.
        HistogramWatcher incognitoShow =
                HistogramWatcher.newSingleRecordWatcher(
                        DownloadToolbarButtonMetrics.SHOWN_HISTOGRAM, /* value= */ 1);
        setIncognitoSelected(true);
        assertLingering();
        setIncognitoSelected(false);
        assertState(DownloadToolbarButtonState.HIDDEN);
        setIncognitoSelected(true);
        assertLingering();
        incognitoShow.assertExpected();
    }
}
