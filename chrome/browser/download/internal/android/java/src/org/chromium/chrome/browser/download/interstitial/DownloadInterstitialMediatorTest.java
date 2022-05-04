// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.DOWNLOAD_ITEM;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.download.home.StubbedOfflineContentProvider;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the {@link DownloadInterstitialMediator}. Modifies the page state through the
 * {@link PropertyModel} and observes changes to the mediator/model.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class DownloadInterstitialMediatorTest {
    private static final String DOWNLOAD_BUTTON_TEXT = "Download";
    private static final String CANCEL_BUTTON_TEXT = "Cancel";
    private static final String RESUME_BUTTON_TEXT = "Resume";
    private static final String OPEN_BUTTON_TEXT = "Open";
    private static final String DELETE_BUTTON_TEXT = "Delete";

    @Mock
    private SnackbarManager mSnackbarManager;

    private final TestOfflineContentProvider mProvider = new TestOfflineContentProvider();
    private DownloadInterstitialMediator mMediator;
    private PropertyModel mModel;
    private OfflineItem mItem0;

    private boolean mSnackbarShown;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        mSnackbarShown = false;
        doAnswer((invocation) -> mSnackbarShown = true)
                .when(mSnackbarManager)
                .showSnackbar(isA(Snackbar.class));
        SharedPreferencesManager sharedPrefsManager = SharedPreferencesManager.getInstance();
        sharedPrefsManager.disableKeyCheckerForTesting();
        mItem0 = createOfflineItem("item0");
        mModel = new PropertyModel.Builder(DownloadInterstitialProperties.ALL_KEYS).build();
        // Set the initial button texts. This is usually done in DownloadInterstitialView.
        mModel.set(DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT, "");
        mModel.set(DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT, CANCEL_BUTTON_TEXT);
        mProvider.addItem(mItem0);
        mMediator = new DownloadInterstitialMediator(InstrumentationRegistry::getContext, mModel,
                mItem0.originalUrl, mProvider, mSnackbarManager, sharedPrefsManager);
        // Increment progress to trigger onItemUpdated method for OfflineContentProvider observers.
        // This attaches the OfflineItem to the mediator.
        mProvider.incrementProgress(mItem0.id);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testItemIsAttached() {
        assertEquals(mItem0, mModel.get(DOWNLOAD_ITEM));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testSecondDownloadNotAttached() {
        OfflineItem item1 = createOfflineItem("item1");
        item1.originalUrl = "www.bar.com";
        mProvider.addItem(item1);
        mProvider.incrementProgress(item1.id);
        assertEquals(mItem0, mModel.get(DOWNLOAD_ITEM));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressDownloadNotAttached() {
        OfflineItem item1 = createOfflineItem("item1");
        item1.originalUrl = "www.bar.com";
        // Remove observer so that the mediator can attach its own observer.
        mProvider.setObserver(null);
        mModel.set(DOWNLOAD_ITEM, null);
        mMediator = new DownloadInterstitialMediator(InstrumentationRegistry::getContext, mModel,
                item1.originalUrl, mProvider, mSnackbarManager,
                SharedPreferencesManager.getInstance());
        mProvider.incrementProgress(mItem0.id);
        mProvider.addItem(item1);
        mProvider.incrementProgress(item1.id);
        assertEquals(item1.id.id, mModel.get(DOWNLOAD_ITEM).id.id);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressItemIsCancelled() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.CANCELLED, mModel.get(STATE));
        assertNotEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressReDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);
        clickButtonWithText(DOWNLOAD_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.IN_PROGRESS, mModel.get(STATE));
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressPauseDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mModel.get(ListProperties.CALLBACK_PAUSE).onResult(mModel.get(DOWNLOAD_ITEM));

        assertEquals(DownloadInterstitialProperties.State.PAUSED, mModel.get(STATE));
        assertEquals(OfflineItemState.PAUSED, mModel.get(DOWNLOAD_ITEM).state);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressResumeDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mModel.get(ListProperties.CALLBACK_PAUSE).bind(mModel.get(DOWNLOAD_ITEM));
        clickButtonWithText(RESUME_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.IN_PROGRESS, mModel.get(STATE));
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testCancelledDownloadIsDeletedAfterClose() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);
        mMediator.destroy();

        assertFalse(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testOpenDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(OPEN_BUTTON_TEXT);

        assertEquals(mModel.get(DOWNLOAD_ITEM).id, mProvider.mLastOpenedDownload);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testDeleteDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);

        assertTrue(mSnackbarShown);
        assertEquals(DownloadInterstitialProperties.State.CANCELLED, mModel.get(STATE));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testReDownloadDeletedDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        clickButtonWithText(DOWNLOAD_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.SUCCESSFUL, mModel.get(STATE));
        assertTrue(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testDeletedDownloadIsRemovedAfterClose() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        mMediator.destroy();

        assertFalse(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
    }

    private void clickButtonWithText(String text) {
        if (mModel.get(DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT).equals(text)) {
            mModel.get(DownloadInterstitialProperties.PRIMARY_BUTTON_CALLBACK)
                    .onResult(mModel.get(DOWNLOAD_ITEM));
        } else if (mModel.get(DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT).equals(text)) {
            mModel.get(DownloadInterstitialProperties.SECONDARY_BUTTON_CALLBACK)
                    .onResult(mModel.get(DOWNLOAD_ITEM));
        }
    }

    private static OfflineItem createOfflineItem(String id) {
        OfflineItem item = new OfflineItem();
        item.id = new ContentId("test", id);
        item.progress = new OfflineItem.Progress(0L, 10L, OfflineItemProgressUnit.BYTES);
        item.state = OfflineItemState.IN_PROGRESS;
        item.title = "Test Item";
        item.description = "Test Description";
        item.originalUrl = "www.foo.com";
        return item;
    }

    /**
     * Extends {@link StubbedOfflineContentProvider} to add some logic to some of the no-op methods
     * for testing.
     */
    private static class TestOfflineContentProvider extends StubbedOfflineContentProvider {
        private ContentId mLastOpenedDownload;

        /** Called to increment the progress of an offline item and notify observers. */
        public void incrementProgress(ContentId id) {
            OfflineItem item = findItem(id);
            item.progress = new OfflineItem.Progress(
                    item.progress.value + 1, item.progress.max, item.progress.unit);
            if (item.progress.value == item.progress.max) {
                item.state = OfflineItemState.COMPLETE;
            }
            notifyObservers(id);
        }

        /**
         * Called to complete the progress of an offline item, update its state and notify
         * observers.
         */
        public void completeDownload(ContentId id) {
            OfflineItem item = findItem(id);
            item.progress = new OfflineItem.Progress(
                    item.progress.max, item.progress.max, item.progress.unit);
            item.state = OfflineItemState.COMPLETE;
            notifyObservers(id);
        }

        @Override
        public void openItem(OpenParams openParams, ContentId id) {
            mLastOpenedDownload = id;
        }

        @Override
        public void pauseDownload(ContentId id) {
            findItem(id).state = OfflineItemState.PAUSED;
            notifyObservers(id);
        }

        @Override
        public void resumeDownload(ContentId id, boolean hasUserGesture) {
            if (findItem(id).state != OfflineItemState.COMPLETE) {
                findItem(id).state = OfflineItemState.IN_PROGRESS;
            }
            notifyObservers(id);
        }

        @Override
        public void cancelDownload(ContentId id) {
            findItem(id).state = OfflineItemState.CANCELLED;
            removeItem(id);
            notifyObservers(id);
        }
    }
}
