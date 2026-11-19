// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link DownloadTrayMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadTrayMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    @Mock private OfflineContentProvider mProvider;
    @Mock private DownloadTrayMediator.PopupController mPopupController;
    @Mock private DownloadTrayDelegate mDelegate;
    @Mock private Runnable mOnActioned;
    @Captor private ArgumentCaptor<Callback<ArrayList<OfflineItem>>> mGetAllItemsCallbackCaptor;
    @Captor private ArgumentCaptor<ModelList> mRowsCaptor;

    private Activity mActivity;
    private View mAnchor;
    private DownloadStateModel mModel;
    private DownloadTrayMediator mMediator;
    private boolean mIsWindowFocused = true;
    private boolean mIsIncognitoSelected;
    private View mCurrentAnchor;

    private int mItemCount;

    @Before
    public void setUp() {
        // A real, attached view so isShown() behaves as in production.
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mAnchor = new View(mActivity);
        mActivity.setContentView(mAnchor);
        mCurrentAnchor = mAnchor;
        doReturn(true).when(mPopupController).showCompletions(any(), any());
        // Like the real popup, the full tray replaces (and so dismisses) a showing completion
        // popup.
        doAnswer(
                        invocation -> {
                            mMediator.onDismissed();
                            return true;
                        })
                .when(mPopupController)
                .showFullTray(any(), any());
        mModel = new DownloadStateModel(mProvider);
        verify(mProvider).getAllItems(mGetAllItemsCallbackCaptor.capture());
        mMediator =
                new DownloadTrayMediator(
                        mActivity,
                        mModel,
                        () -> mCurrentAnchor,
                        () -> mIsWindowFocused,
                        () -> mIsIncognitoSelected,
                        mOnActioned,
                        mDelegate,
                        mPopupController);
    }

    private static ContentId id(String guid) {
        return LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, guid);
    }

    private OfflineItem createItem(String guid, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id(guid);
        item.title = guid + ".pdf";
        item.state = state;
        item.totalSizeBytes = 4_300_000;
        // Started now, each later than the one before, so the list order is defined.
        item.creationTimeMs = TimeUtils.currentTimeMillis() + mItemCount++;
        return item;
    }

    private OfflineItem createIncognitoItem(
            String guid, @OfflineItemState int state, OtrProfileId profileId) {
        OfflineItem item = createItem(guid, state);
        item.isOffTheRecord = true;
        item.otrProfileId = OtrProfileId.serialize(profileId);
        return item;
    }

    /** Reports {@code guid} in progress, then in {@code endState}; returns the final item. */
    private OfflineItem runToState(String guid, @OfflineItemState int endState) {
        mModel.onItemsAdded(List.of(createItem(guid, OfflineItemState.IN_PROGRESS)));
        OfflineItem ended = createItem(guid, endState);
        mModel.onItemUpdated(ended, /* updateDelta= */ null);
        return ended;
    }

    /** Simulates the popup closing, e.g. auto-dismissing, and the min interval passing. */
    private void dismissAndWait() {
        mMediator.onDismissed();
        mFakeTime.advanceMillis(DownloadTrayMediator.MIN_SHOW_INTERVAL_MS);
    }

    /** Returns the rows of the most recently shown popup. */
    private ModelList captureShownRows() {
        verify(mPopupController, atLeastOnce()).showCompletions(eq(mAnchor), mRowsCaptor.capture());
        return mRowsCaptor.getValue();
    }

    private PropertyModel captureShownRow() {
        ModelList rows = captureShownRows();
        assertEquals(1, rows.size());
        return rows.get(0).model;
    }

    /** Returns the rows of the most recently shown full tray. */
    private ModelList captureFullTrayRows() {
        verify(mPopupController, atLeastOnce()).showFullTray(eq(mAnchor), mRowsCaptor.capture());
        return mRowsCaptor.getValue();
    }

    private static List<CharSequence> titles(ModelList rows) {
        List<CharSequence> titles = new ArrayList<>();
        for (ListItem row : rows) {
            titles.add(row.model.get(DownloadTrayItemProperties.TITLE));
        }
        return titles;
    }

    @Test
    public void testCompletion_showsRow() {
        runToState("report", OfflineItemState.COMPLETE);

        PropertyModel row = captureShownRow();
        assertEquals(R.drawable.ic_drive_file_24dp, row.get(DownloadTrayItemProperties.ICON));
        assertEquals("report.pdf", row.get(DownloadTrayItemProperties.TITLE));
        assertEquals("4.30 MB • Done", row.get(DownloadTrayItemProperties.DESCRIPTION));
    }

    @Test
    public void testNoPopup_unlessFocusedWindowSawItCompleteSafely() {
        // Unsuccessful ends.
        runToState("cancelled", OfflineItemState.CANCELLED);
        runToState("failed", OfflineItemState.FAILED);
        runToState("interrupted", OfflineItemState.INTERRUPTED);
        // Completions this window never saw in progress, e.g. from the initial snapshot.
        mGetAllItemsCallbackCaptor
                .getValue()
                .onResult(new ArrayList<>(List.of(createItem("old", OfflineItemState.COMPLETE))));
        mModel.onItemUpdated(
                createItem("unseen", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        // Dangerous downloads, which have their own warning UI.
        mModel.onItemsAdded(List.of(createItem("dangerous", OfflineItemState.IN_PROGRESS)));
        OfflineItem dangerous = createItem("dangerous", OfflineItemState.COMPLETE);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemUpdated(dangerous, /* updateDelta= */ null);
        // Unfocused window, missing anchor, hidden anchor.
        mIsWindowFocused = false;
        runToState("unfocused", OfflineItemState.COMPLETE);
        mIsWindowFocused = true;
        mCurrentAnchor = null;
        runToState("no-anchor", OfflineItemState.COMPLETE);
        mCurrentAnchor = mAnchor;
        mAnchor.setVisibility(View.GONE);
        runToState("hidden-anchor", OfflineItemState.COMPLETE);

        verify(mPopupController, never()).showCompletions(any(), any());
    }

    @Test
    public void testCompletionWhileOthersDownloading_waitsUntilAllDoneThenListsBoth() {
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        verify(mPopupController, never()).showCompletions(any(), any());

        // Most recently started first, as on desktop.
        mModel.onItemUpdated(createItem("b", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        assertEquals(List.of("b.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testPausedPendingOrDangerousDownloads_doNotDelayPopup() {
        // As on desktop, paused downloads, pending ones (waiting to auto-resume) and dangerous ones
        // awaiting a decision don't block.
        OfflineItem dangerous = createItem("dangerous", OfflineItemState.IN_PROGRESS);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("paused", OfflineItemState.PAUSED),
                        createItem("pending", OfflineItemState.PENDING),
                        dangerous));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        assertEquals("a.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testLastActiveDownloadPausedThenFlaggedDangerous_showsWaitingCompletion() {
        // "a" completes while "b" runs.
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);

        // As on desktop, pausing "b" doesn't end it, so "a" keeps waiting.
        mModel.onItemUpdated(createItem("b", OfflineItemState.PAUSED), /* updateDelta= */ null);
        verify(mPopupController, never()).showCompletions(any(), any());

        // Once resumed, "b" being flagged as dangerous ends it, as on desktop.
        mModel.onItemUpdated(
                createItem("b", OfflineItemState.IN_PROGRESS), /* updateDelta= */ null);
        OfflineItem dangerous = createItem("b", OfflineItemState.IN_PROGRESS);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemUpdated(dangerous, /* updateDelta= */ null);
        assertEquals("a.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testLastActiveDownloadFailingOrRemoved_showsWaitingCompletions() {
        // "a" completes while "b" runs; "b" then fails.
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("b", OfflineItemState.FAILED), /* updateDelta= */ null);
        assertEquals("a.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));

        // "c" completes while "d" runs; "d" is then removed, e.g. cancelled. "a", not acted on, is
        // listed again, as on desktop.
        dismissAndWait();
        mModel.onItemsAdded(
                List.of(
                        createItem("c", OfflineItemState.IN_PROGRESS),
                        createItem("d", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("c", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemRemoved(id("d"));
        assertEquals(List.of("c.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testIncognitoDownloads_onlyShownWhileIncognitoTabsAreShown() {
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS),
                        createIncognitoItem("c", OfflineItemState.IN_PROGRESS, primary),
                        createIncognitoItem("d", OfflineItemState.IN_PROGRESS, primary)));

        // Showing regular tabs, an incognito download is neither listed with a waiting regular one
        // nor holds it back.
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemUpdated(
                createIncognitoItem("c", OfflineItemState.COMPLETE, primary),
                /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("b", OfflineItemState.FAILED), /* updateDelta= */ null);
        assertEquals("a.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));

        // Showing incognito tabs, downloads from their profile and regular ones show (as desktop's
        // incognito bubble), and other incognito profiles' (e.g. incognito Custom Tabs') don't hold
        // them back.
        dismissAndWait();
        mIsIncognitoSelected = true;
        mModel.onItemsAdded(
                List.of(
                        createIncognitoItem(
                                "cct",
                                OfflineItemState.IN_PROGRESS,
                                new OtrProfileId("CCT:Incognito-1"))));
        mModel.onItemUpdated(
                createIncognitoItem("d", OfflineItemState.COMPLETE, primary),
                /* updateDelta= */ null);
        assertEquals(List.of("d.pdf", "c.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testSwitchingToRegularTabsBeforeShowing_dropsIncognitoCompletion() {
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        mIsIncognitoSelected = true;
        mModel.onItemsAdded(
                List.of(
                        createIncognitoItem("a", OfflineItemState.IN_PROGRESS, primary),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(
                createIncognitoItem("a", OfflineItemState.COMPLETE, primary),
                /* updateDelta= */ null);

        mIsIncognitoSelected = false;
        mModel.onItemUpdated(createItem("b", OfflineItemState.FAILED), /* updateDelta= */ null);
        verify(mPopupController, never()).showCompletions(any(), any());
    }

    @Test
    public void testWaitingCompletionRemovedOrRetried_isNotShown() {
        // An earlier, un-actioned completion alone doesn't warrant a popup: only a new one does.
        runToState("earlier", OfflineItemState.COMPLETE);
        dismissAndWait();

        // "a" completes while "b" runs, then is removed; "c" completes, then is retried and fails.
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS),
                        createItem("c", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemRemoved(id("a"));
        mModel.onItemUpdated(createItem("c", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemUpdated(
                createItem("c", OfflineItemState.IN_PROGRESS), /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("c", OfflineItemState.FAILED), /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("b", OfflineItemState.FAILED), /* updateDelta= */ null);
        verify(mPopupController, times(1)).showCompletions(any(), any());
    }

    @Test
    public void testOpen_marksActionedDismissesAndOpens() {
        OfflineItem item = runToState("report", OfflineItemState.COMPLETE);

        captureShownRow().get(DownloadTrayItemProperties.ON_OPEN).onClick(mAnchor);

        InOrder inOrder = inOrder(mOnActioned, mPopupController, mDelegate);
        inOrder.verify(mOnActioned).run();
        inOrder.verify(mPopupController).dismiss();
        inOrder.verify(mDelegate).openItem(item);

        // Acted on, it isn't listed with the next completion.
        dismissAndWait();
        runToState("next", OfflineItemState.COMPLETE);
        assertEquals("next.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testCompletionsWithinMinInterval_showOnce() {
        runToState("a", OfflineItemState.COMPLETE);
        mMediator.onDismissed();
        mFakeTime.advanceMillis(DownloadTrayMediator.MIN_SHOW_INTERVAL_MS - 1);
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(1)).showCompletions(any(), any());

        // Once allowed again, the completion that was held back is listed too.
        mFakeTime.advanceMillis(1);
        runToState("c", OfflineItemState.COMPLETE);
        assertEquals(List.of("c.pdf", "b.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testCompletionWhilePopupShowing_isListedNextTime() {
        runToState("a", OfflineItemState.COMPLETE);

        // As on desktop, the showing popup isn't replaced, even once the min interval has passed.
        mFakeTime.advanceMillis(DownloadTrayMediator.MIN_SHOW_INTERVAL_MS);
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(1)).showCompletions(any(), any());

        dismissAndWait();
        runToState("c", OfflineItemState.COMPLETE);
        assertEquals(List.of("c.pdf", "b.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testCompletionNoLongerRecent_isForgotten() {
        runToState("a", OfflineItemState.COMPLETE);

        // Past the model's recent window, "a" is no longer listed and, as on desktop, counts as
        // acted on from then on.
        dismissAndWait();
        mFakeTime.advanceMillis(TimeUnit.HOURS.toMillis(24));
        runToState("b", OfflineItemState.COMPLETE);
        assertEquals("b.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testPopupThatCouldNotShow_doesNotStartMinInterval() {
        // E.g. no room for the popup: the next completion must not be rate-limited.
        doReturn(false).when(mPopupController).showCompletions(any(), any());
        runToState("a", OfflineItemState.COMPLETE);
        mModel.onItemRemoved(id("a"));
        verify(mPopupController, never()).dismiss();

        doReturn(true).when(mPopupController).showCompletions(any(), any());
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(2)).showCompletions(any(), any());
    }

    @Test
    public void testShownRows_updateInPlaceAndRemovalDismissesOnceEmpty() {
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(createItem("a", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("b", OfflineItemState.COMPLETE), /* updateDelta= */ null);
        ModelList rows = captureShownRows();
        assertEquals(List.of("b.pdf", "a.pdf"), titles(rows));

        // E.g. renamed from the Downloads page while the popup is showing.
        OfflineItem renamed = createItem("a", OfflineItemState.COMPLETE);
        renamed.title = "renamed.pdf";
        mModel.onItemUpdated(renamed, /* updateDelta= */ null);
        assertEquals(List.of("b.pdf", "renamed.pdf"), titles(rows));

        // E.g. retried, so no longer a completed download, or deleted from the Downloads page;
        // the popup closes once nothing is left in it.
        mModel.onItemUpdated(
                createItem("b", OfflineItemState.IN_PROGRESS), /* updateDelta= */ null);
        mModel.onItemRemoved(id("other"));
        assertEquals(List.of("renamed.pdf"), titles(rows));
        verify(mPopupController, never()).dismiss();
        mModel.onItemRemoved(id("a"));
        verify(mPopupController).dismiss();

        // Once dismissed, a later removal of the same item doesn't dismiss again.
        clearInvocations(mPopupController);
        mMediator.onDismissed();
        mModel.onItemRemoved(id("a"));
        verify(mPopupController, never()).dismiss();
    }

    @Test
    public void testDestroy_dismissesAndStopsObserving() {
        runToState("a", OfflineItemState.COMPLETE);

        // A showing popup must not outlive the mediator (and the window it is anchored to).
        mMediator.destroy();
        verify(mPopupController).dismiss();

        dismissAndWait();
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(1)).showCompletions(any(), any());
    }

    @Test
    public void testFullTray_listsAllRecentNewestFirstAndToggles() {
        // "a" was shown and not acted on; "b" was opened from its popup (which lists both).
        runToState("a", OfflineItemState.COMPLETE);
        dismissAndWait();
        runToState("b", OfflineItemState.COMPLETE);
        captureShownRows().get(0).model.get(DownloadTrayItemProperties.ON_OPEN).onClick(mAnchor);
        dismissAndWait();

        // The full tray lists both, newest first, and needs no new completion to open.
        mCurrentAnchor = null;
        mMediator.toggleFullTray();
        verify(mPopupController, never()).showFullTray(any(), any());
        mCurrentAnchor = mAnchor;
        mMediator.toggleFullTray();
        assertEquals(List.of("b.pdf", "a.pdf"), titles(captureFullTrayRows()));

        // Toggling again closes it.
        clearInvocations(mPopupController);
        mMediator.toggleFullTray();
        verify(mPopupController).dismiss();
    }

    @Test
    public void testFullTray_listsInterruptedDownloadsUntilCancelled() {
        runToState("done", OfflineItemState.COMPLETE);
        dismissAndWait();
        runToState("interrupted", OfflineItemState.INTERRUPTED);
        runToState("failed", OfflineItemState.FAILED);
        runToState("cancelled", OfflineItemState.CANCELLED);

        // As on the Downloads page, interrupted downloads are listed, failed and cancelled ones
        // aren't. With no file to open, the row only says the download failed.
        mMediator.toggleFullTray();
        ModelList rows = captureFullTrayRows();
        assertEquals(List.of("interrupted.pdf", "done.pdf"), titles(rows));
        PropertyModel interrupted = rows.get(0).model;
        assertEquals(
                R.drawable.ic_file_download_off_24dp,
                interrupted.get(DownloadTrayItemProperties.ICON));
        assertEquals(
                mActivity.getString(R.string.download_tray_item_status_failed),
                interrupted.get(DownloadTrayItemProperties.DESCRIPTION));
        assertNull(interrupted.get(DownloadTrayItemProperties.ON_OPEN));

        // Retried, its row shows the download in progress where it is; cancelled, it leaves the
        // tray.
        mModel.onItemUpdated(
                createItem("interrupted", OfflineItemState.IN_PROGRESS), /* updateDelta= */ null);
        assertEquals(List.of("interrupted.pdf", "done.pdf"), titles(rows));
        assertTrue(interrupted.get(DownloadTrayItemProperties.IS_IN_PROGRESS));
        mModel.onItemUpdated(
                createItem("interrupted", OfflineItemState.CANCELLED), /* updateDelta= */ null);
        assertEquals(List.of("done.pdf"), titles(rows));
    }

    @Test
    public void testFullTray_listsRunningThenPausedThenEndedDownloads() {
        runToState("done", OfflineItemState.COMPLETE);
        dismissAndWait();
        OfflineItem dangerous = createItem("dangerous", OfflineItemState.IN_PROGRESS);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("paused", OfflineItemState.PAUSED),
                        createItem("pending", OfflineItemState.PENDING),
                        createItem("b", OfflineItemState.IN_PROGRESS),
                        dangerous,
                        createIncognitoItem(
                                "incognito",
                                OfflineItemState.IN_PROGRESS,
                                OtrProfileId.getPrimaryOtrProfileId())));

        // As on desktop: running (or waiting to), then paused, then ended, each most recently
        // started first. Dangerous downloads and ones the window doesn't show aren't listed.
        mMediator.toggleFullTray();
        ModelList rows = captureFullTrayRows();
        assertEquals(
                List.of("b.pdf", "pending.pdf", "a.pdf", "paused.pdf", "done.pdf"), titles(rows));
    }

    @Test
    public void testFullTray_capsRowsAcrossInProgressAndEndedDownloads() {
        runToState("older", OfflineItemState.COMPLETE);
        mMediator.onDismissed();
        runToState("newer", OfflineItemState.COMPLETE);
        mMediator.onDismissed();
        List<OfflineItem> active = new ArrayList<>();
        for (int i = 0; i < DownloadStateModel.MAX_RECENT_ITEMS - 1; i++) {
            active.add(createItem("active" + i, OfflineItemState.IN_PROGRESS));
        }
        mModel.onItemsAdded(active);

        // As on desktop, one cap covers all rows, so only the newer ended download fits.
        mMediator.toggleFullTray();
        ModelList rows = captureFullTrayRows();
        assertEquals(DownloadStateModel.MAX_RECENT_ITEMS, rows.size());
        assertEquals(
                "newer.pdf", rows.get(rows.size() - 1).model.get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testInProgressRow_showsProgressAndFollowsDownload() {
        OfflineItem item = createItem("a", OfflineItemState.IN_PROGRESS);
        item.receivedBytes = 2_150_000;
        item.timeRemainingMs = TimeUnit.SECONDS.toMillis(15);
        mModel.onItemsAdded(List.of(item));
        mMediator.toggleFullTray();
        PropertyModel row = captureFullTrayRows().get(0).model;
        assertEquals(
                "2.15 MB / 4.30 MB • 15 secs left",
                row.get(DownloadTrayItemProperties.DESCRIPTION));
        assertEquals(50, row.get(DownloadTrayItemProperties.PROGRESS));
        assertFalse(row.get(DownloadTrayItemProperties.IS_PAUSED));
        assertNull(row.get(DownloadTrayItemProperties.ON_OPEN)); // Nothing to open yet.

        // The row follows the download while the tray is open. Without a time left, as on the
        // Downloads page, only the sizes show.
        item = createItem("a", OfflineItemState.IN_PROGRESS);
        item.receivedBytes = 3_440_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals("3.44 MB / 4.30 MB", row.get(DownloadTrayItemProperties.DESCRIPTION));
        assertEquals(80, row.get(DownloadTrayItemProperties.PROGRESS));

        // Fractional percentages round to the nearest integer (3.60 / 4.30 is ~83.7%).
        item = createItem("a", OfflineItemState.IN_PROGRESS);
        item.receivedBytes = 3_600_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals(84, row.get(DownloadTrayItemProperties.PROGRESS));

        // More than the announced size fills the bar, no further.
        item = createItem("a", OfflineItemState.IN_PROGRESS);
        item.receivedBytes = 5_000_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals(100, row.get(DownloadTrayItemProperties.PROGRESS));

        item = createItem("a", OfflineItemState.PAUSED);
        item.receivedBytes = 3_440_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals("3.44 MB / 4.30 MB • Paused", row.get(DownloadTrayItemProperties.DESCRIPTION));
        assertTrue(row.get(DownloadTrayItemProperties.IS_PAUSED));

        // With no size known, the bar animates only while downloading, and a download that has
        // received nothing shows its state alone.
        item = createItem("a", OfflineItemState.IN_PROGRESS);
        item.totalSizeBytes = 0;
        item.receivedBytes = 1_500_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals("1.50 MB / ?", row.get(DownloadTrayItemProperties.DESCRIPTION));
        assertEquals(
                DownloadTrayItemProperties.PROGRESS_INDETERMINATE,
                row.get(DownloadTrayItemProperties.PROGRESS));

        item = createItem("a", OfflineItemState.PENDING);
        item.totalSizeBytes = 0;
        mModel.onItemUpdated(item, /* updateDelta= */ null);
        assertEquals("Pending", row.get(DownloadTrayItemProperties.DESCRIPTION));
        assertEquals(0, row.get(DownloadTrayItemProperties.PROGRESS));
    }

    @Test
    public void testInProgressRow_becomesEndedRowInPlaceOrLeavesTheTray() {
        runToState("done", OfflineItemState.COMPLETE);
        mMediator.onDismissed();
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        mMediator.toggleFullTray();
        ModelList rows = captureFullTrayRows();
        assertEquals(List.of("b.pdf", "a.pdf", "done.pdf"), titles(rows));

        // Completing while the tray is open, "b" keeps its place as an ended row. As on desktop, a
        // download started meanwhile isn't added.
        OfflineItem completed = createItem("b", OfflineItemState.COMPLETE);
        mModel.onItemUpdated(completed, /* updateDelta= */ null);
        mModel.onItemsAdded(List.of(createItem("late", OfflineItemState.PAUSED)));
        assertEquals(List.of("b.pdf", "a.pdf", "done.pdf"), titles(rows));
        assertFalse(rows.get(0).model.get(DownloadTrayItemProperties.IS_IN_PROGRESS));
        assertEquals(
                "4.30 MB • Done", rows.get(0).model.get(DownloadTrayItemProperties.DESCRIPTION));

        // Flagged as dangerous, "a" gets its own warning UI instead of a row. Nothing is
        // downloading any more, yet as on desktop no completion popup replaces the open tray.
        OfflineItem dangerous = createItem("a", OfflineItemState.IN_PROGRESS);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemUpdated(dangerous, /* updateDelta= */ null);
        assertEquals(List.of("b.pdf", "done.pdf"), titles(rows));
        verify(mPopupController, times(1)).showCompletions(any(), any());

        rows.get(0).model.get(DownloadTrayItemProperties.ON_OPEN).onClick(mAnchor);
        verify(mDelegate).openItem(completed);
    }

    @Test
    public void testFullTray_skipsDangerousDownloads() {
        runToState("safe", OfflineItemState.COMPLETE);
        OfflineItem dangerous = runToState("dangerous", OfflineItemState.COMPLETE);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        mModel.onItemUpdated(dangerous, /* updateDelta= */ null);
        dismissAndWait();

        // Dangerous downloads have their own warning UI rather than a row.
        mMediator.toggleFullTray();
        assertEquals(List.of("safe.pdf"), titles(captureFullTrayRows()));
    }

    @Test
    public void testFullTray_replacesCompletionPopupAndClosesOnceEmpty() {
        runToState("a", OfflineItemState.COMPLETE);

        // Opening the tray replaces the completion popup.
        mMediator.toggleFullTray();
        assertEquals(List.of("a.pdf"), titles(captureFullTrayRows()));

        // As on desktop, the tray closes once its last row is gone.
        verify(mPopupController, never()).dismiss();
        mModel.onItemRemoved(id("a"));
        assertEquals(0, captureFullTrayRows().size());
        verify(mPopupController).dismiss();
    }

    @Test
    public void testFullTray_withNothingToList_opensDownloadManagerForWindowProfile() {
        // As on desktop, the button opens the full download history rather than an empty tray.
        mIsIncognitoSelected = true;
        mMediator.toggleFullTray();

        verify(mPopupController, never()).showFullTray(any(), any());
        verify(mDelegate).showDownloadManager(/* isIncognito= */ true);
    }

    @Test
    public void testFullTray_letsNextCompletionPopupShowWithoutWaiting() {
        runToState("a", OfflineItemState.COMPLETE);
        mMediator.onDismissed();

        // As on desktop, opening the tray resets the minimum interval between completion popups.
        mMediator.toggleFullTray();
        mMediator.onDismissed();
        runToState("b", OfflineItemState.COMPLETE);

        verify(mPopupController, times(2)).showCompletions(any(), any());
        assertEquals("b.pdf", captureShownRow().get(DownloadTrayItemProperties.TITLE));
    }

    @Test
    public void testFullTrayThatCouldNotShow_changesNothing() {
        runToState("a", OfflineItemState.COMPLETE);
        mMediator.onDismissed();
        doReturn(false).when(mPopupController).showFullTray(any(), any());

        // Not showing, e.g. for lack of room, the tray isn't taken to be open, so the next press
        // tries to open it again rather than close it.
        mMediator.toggleFullTray();
        mMediator.toggleFullTray();
        verify(mPopupController, times(2)).showFullTray(any(), any());
        verify(mPopupController, never()).dismiss();

        // Nor does the download it would have listed count as acted on.
        runToState("b", OfflineItemState.COMPLETE);
        assertEquals(List.of("b.pdf", "a.pdf"), titles(captureShownRows()));
    }

    @Test
    public void testFullTray_listsAndActionsOnlyDownloadsTheWindowShows() {
        // A regular download, then an incognito one while regular tabs are shown.
        runToState("a", OfflineItemState.COMPLETE);
        OtrProfileId primary = OtrProfileId.getPrimaryOtrProfileId();
        mModel.onItemsAdded(
                List.of(createIncognitoItem("b", OfflineItemState.IN_PROGRESS, primary)));
        mModel.onItemUpdated(
                createIncognitoItem("b", OfflineItemState.COMPLETE, primary),
                /* updateDelta= */ null);
        mMediator.onDismissed();

        mMediator.toggleFullTray();
        assertEquals(List.of("a.pdf"), titles(captureFullTrayRows()));
        mMediator.onDismissed();

        // As on desktop, only the listed download counts as acted on, so once incognito tabs are
        // shown the next completion popup still lists the incognito one.
        mIsIncognitoSelected = true;
        runToState("c", OfflineItemState.COMPLETE);
        assertEquals(List.of("c.pdf", "b.pdf"), titles(captureShownRows()));

        // With incognito tabs shown, the tray lists both profiles' downloads.
        mMediator.onDismissed();
        mMediator.toggleFullTray();
        assertEquals(List.of("c.pdf", "b.pdf", "a.pdf"), titles(captureFullTrayRows()));
    }

    @Test
    public void testDescription_showsTimeSinceCompletionAfterAMinute() {
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        OfflineItem justDone = createItem("a", OfflineItemState.COMPLETE);
        justDone.completionTimeMs = TimeUtils.currentTimeMillis() - TimeUnit.SECONDS.toMillis(59);
        mModel.onItemUpdated(justDone, /* updateDelta= */ null);
        OfflineItem earlier = createItem("b", OfflineItemState.COMPLETE);
        earlier.completionTimeMs = TimeUtils.currentTimeMillis() - TimeUnit.MINUTES.toMillis(3);
        mModel.onItemUpdated(earlier, /* updateDelta= */ null);

        ModelList rows = captureShownRows();
        assertEquals(
                "4.30 MB • 3 minutes ago",
                rows.get(0).model.get(DownloadTrayItemProperties.DESCRIPTION));
        assertEquals(
                "4.30 MB • Done", rows.get(1).model.get(DownloadTrayItemProperties.DESCRIPTION));
    }

    @Test
    public void testDescription_keepsSizesLeftToRightInRightToLeftLocales() {
        Locale defaultLocale = Locale.getDefault();
        Locale.setDefault(new Locale("he"));
        try {
            OfflineItem item = createItem("a", OfflineItemState.PAUSED);
            item.receivedBytes = 2_150_000;
            mModel.onItemsAdded(List.of(item));
            mMediator.toggleFullTray();
            PropertyModel row = captureFullTrayRows().get(0).model;

            // The sizes are embedded as left-to-right text (U+202A ... U+202C), as on desktop.
            String description = row.get(DownloadTrayItemProperties.DESCRIPTION).toString();
            assertTrue(description, description.contains("\u202A2.15 MB / 4.30 MB\u202C"));
        } finally {
            Locale.setDefault(defaultLocale);
        }
    }

    @Test
    public void testDescription_unknownTotalUsesReceivedBytes() {
        mModel.onItemsAdded(List.of(createItem("a", OfflineItemState.IN_PROGRESS)));
        OfflineItem item = createItem("a", OfflineItemState.COMPLETE);
        item.totalSizeBytes = 0;
        item.receivedBytes = 1_500_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);

        assertEquals(
                "1.50 MB • Done", captureShownRow().get(DownloadTrayItemProperties.DESCRIPTION));
    }
}
