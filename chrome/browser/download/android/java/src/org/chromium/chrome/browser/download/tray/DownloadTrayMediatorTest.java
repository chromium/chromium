// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

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
    @Captor private ArgumentCaptor<PropertyModel> mRowCaptor;

    private Activity mActivity;
    private View mAnchor;
    private DownloadStateModel mModel;
    private DownloadTrayMediator mMediator;
    private boolean mIsWindowFocused = true;
    private View mCurrentAnchor;

    @Before
    public void setUp() {
        // A real, attached view so isShown() behaves as in production.
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mAnchor = new View(mActivity);
        mActivity.setContentView(mAnchor);
        mCurrentAnchor = mAnchor;
        // Like the real popup, showing replaces (and so dismisses) any popup already showing.
        doAnswer(
                        invocation -> {
                            mMediator.onDismissed();
                            return null;
                        })
                .when(mPopupController)
                .showCompletion(any(), any());
        mModel = new DownloadStateModel(mProvider);
        verify(mProvider).getAllItems(mGetAllItemsCallbackCaptor.capture());
        mMediator =
                new DownloadTrayMediator(
                        mActivity,
                        mModel,
                        () -> mCurrentAnchor,
                        () -> mIsWindowFocused,
                        mOnActioned,
                        mDelegate,
                        mPopupController);
    }

    private static ContentId id(String guid) {
        return LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, guid);
    }

    private static OfflineItem createItem(String guid, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id(guid);
        item.title = guid + ".pdf";
        item.state = state;
        item.totalSizeBytes = 4_300_000;
        return item;
    }

    /** Reports {@code guid} in progress, then in {@code endState}; returns the final item. */
    private OfflineItem runToState(String guid, @OfflineItemState int endState) {
        mModel.onItemsAdded(List.of(createItem(guid, OfflineItemState.IN_PROGRESS)));
        OfflineItem ended = createItem(guid, endState);
        mModel.onItemUpdated(ended, /* updateDelta= */ null);
        return ended;
    }

    private PropertyModel captureShownRow() {
        verify(mPopupController).showCompletion(eq(mAnchor), mRowCaptor.capture());
        return mRowCaptor.getValue();
    }

    @Test
    public void testCompletion_showsRow() {
        runToState("report", OfflineItemState.COMPLETE);

        PropertyModel row = captureShownRow();
        assertEquals(R.drawable.ic_drive_file_24dp, row.get(DownloadTrayItemProperties.ICON));
        assertEquals("report.pdf", row.get(DownloadTrayItemProperties.TITLE));
        assertEquals(
                "Download complete • 4.30 MB", row.get(DownloadTrayItemProperties.DESCRIPTION));
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

        verify(mPopupController, never()).showCompletion(any(), any());
    }

    @Test
    public void testOpen_marksActionedDismissesAndOpens() {
        OfflineItem item = runToState("report", OfflineItemState.COMPLETE);

        captureShownRow().get(DownloadTrayItemProperties.ON_OPEN).onClick(mAnchor);

        InOrder inOrder = inOrder(mOnActioned, mPopupController, mDelegate);
        inOrder.verify(mOnActioned).run();
        inOrder.verify(mPopupController).dismiss();
        inOrder.verify(mDelegate).openItem(item);
    }

    @Test
    public void testCompletionsWithinMinInterval_showOnce() {
        runToState("a", OfflineItemState.COMPLETE);
        mFakeTime.advanceMillis(DownloadTrayMediator.MIN_SHOW_INTERVAL_MS - 1);
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(1)).showCompletion(any(), any());

        mFakeTime.advanceMillis(1);
        runToState("c", OfflineItemState.COMPLETE);
        verify(mPopupController, times(2)).showCompletion(any(), any());
    }

    @Test
    public void testRemovingShownItem_dismisses() {
        runToState("a", OfflineItemState.COMPLETE);
        mModel.onItemRemoved(id("other"));
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

        mFakeTime.advanceMillis(DownloadTrayMediator.MIN_SHOW_INTERVAL_MS);
        runToState("b", OfflineItemState.COMPLETE);
        verify(mPopupController, times(1)).showCompletion(any(), any());
    }

    @Test
    public void testDescription_unknownTotalUsesReceivedBytes() {
        mModel.onItemsAdded(List.of(createItem("a", OfflineItemState.IN_PROGRESS)));
        OfflineItem item = createItem("a", OfflineItemState.COMPLETE);
        item.totalSizeBytes = 0;
        item.receivedBytes = 1_500_000;
        mModel.onItemUpdated(item, /* updateDelta= */ null);

        assertEquals(
                "Download complete • 1.50 MB",
                captureShownRow().get(DownloadTrayItemProperties.DESCRIPTION));
    }

    @Test
    public void testIconForItem_matchesDownloadsPage() {
        int[][] expected = {
            {OfflineItemFilter.PAGE, R.drawable.ic_globe_24dp},
            {OfflineItemFilter.VIDEO, R.drawable.ic_videocam_fill_24dp},
            {OfflineItemFilter.AUDIO, R.drawable.ic_music_note_24dp},
            {OfflineItemFilter.IMAGE, R.drawable.ic_drive_image_24dp},
            // The Downloads page also shows the generic icon for documents.
            {OfflineItemFilter.DOCUMENT, R.drawable.ic_drive_file_24dp},
            {OfflineItemFilter.OTHER, R.drawable.ic_drive_file_24dp},
        };
        for (int[] pair : expected) {
            OfflineItem item = createItem("a", OfflineItemState.COMPLETE);
            item.filter = pair[0];
            assertEquals("Filter " + pair[0], pair[1], DownloadTrayMediator.getIconForItem(item));
        }
    }
}
