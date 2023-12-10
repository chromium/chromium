// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.CANCELLED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.COMPLETED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.DELETED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.INITIATED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.OPENED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.PAUSED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.REINITIATED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.RENAMED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.RESUMED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialMediator.UmaHelper.Action.SHARED;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.DOWNLOAD_ITEM;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.download.home.StubbedOfflineContentProvider;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for the {@link DownloadInterstitialMediator}. Modifies the page state through the
 * {@link PropertyModel} and observes changes to the mediator/model.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DownloadInterstitialMediatorTest {
    private static final String DOWNLOAD_BUTTON_TEXT = "Download";
    private static final String CANCEL_BUTTON_TEXT = "Cancel";
    private static final String RESUME_BUTTON_TEXT = "Resume";
    private static final String OPEN_BUTTON_TEXT = "Open";
    private static final String DELETE_BUTTON_TEXT = "Delete";

    @Mock private SnackbarManager mSnackbarManager;

    private final TestOfflineContentProvider mProvider = new TestOfflineContentProvider();
    private FakeModalDialogManager mModalDialogManager;
    private DownloadInterstitialMediator mMediator;
    private UmaTestingHelper mUmaTestingHelper = new UmaTestingHelper();
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
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mUmaTestingHelper.initialise();
        mItem0 = createOfflineItem("item0");
        mModel = new PropertyModel.Builder(DownloadInterstitialProperties.ALL_KEYS).build();
        // Set the initial button texts. This is usually done in DownloadInterstitialView.
        mModel.set(DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT, "");
        mModel.set(DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT, CANCEL_BUTTON_TEXT);
        mModel.set(DownloadInterstitialProperties.RELOAD_TAB, this::reloadTab);
        mProvider.addItem(mItem0);
        mMediator =
                new DownloadInterstitialMediator(
                        ApplicationProvider::getApplicationContext,
                        mModel,
                        mItem0.originalUrl.getSpec(),
                        mProvider,
                        mSnackbarManager,
                        mModalDialogManager);
        // Increment progress to trigger onItemUpdated method for OfflineContentProvider observers.
        // This attaches the OfflineItem to the mediator.
        mProvider.incrementProgress(mItem0.id);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testItemIsAttached() {
        assertEquals(mItem0, mModel.get(DOWNLOAD_ITEM));
        mUmaTestingHelper.assertActionWasLogged(INITIATED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testSecondDownloadNotAttached() {
        OfflineItem item1 = createOfflineItem("item1");
        item1.originalUrl = JUnitTestGURLs.URL_1;
        mProvider.addItem(item1);
        mProvider.incrementProgress(item1.id);
        assertEquals(mItem0, mModel.get(DOWNLOAD_ITEM));
        mUmaTestingHelper.assertActionWasLogged(INITIATED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressDownloadNotAttached() {
        OfflineItem item1 = createOfflineItem("item1");
        item1.originalUrl = JUnitTestGURLs.URL_1;
        // Remove observer so that the mediator can attach its own observer.
        mProvider.setObserver(null);
        mModel.set(DOWNLOAD_ITEM, null);
        mMediator =
                new DownloadInterstitialMediator(
                        ApplicationProvider::getApplicationContext,
                        mModel,
                        item1.originalUrl.getSpec(),
                        mProvider,
                        mSnackbarManager,
                        mModalDialogManager);
        mProvider.incrementProgress(mItem0.id);
        mProvider.addItem(item1);
        mProvider.incrementProgress(item1.id);
        assertEquals(item1.id.id, mModel.get(DOWNLOAD_ITEM).id.id);
        mUmaTestingHelper.assertActionWasLogged(INITIATED, 2);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressItemIsCancelled() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.CANCELLED, mModel.get(STATE));
        assertNotEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mUmaTestingHelper.assertActionWasLogged(CANCELLED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressReDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);
        clickButtonWithText(DOWNLOAD_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.PENDING, mModel.get(STATE));
        // Increment the reloaded item to move to in progress state.
        mProvider.incrementProgress(new ContentId("test", "reloaded-item"));

        assertEquals(DownloadInterstitialProperties.State.IN_PROGRESS, mModel.get(STATE));
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mUmaTestingHelper.assertActionWasLogged(CANCELLED, 1);
        mUmaTestingHelper.assertActionWasLogged(REINITIATED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressPauseDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mModel.get(ListProperties.CALLBACK_PAUSE).onResult(mModel.get(DOWNLOAD_ITEM));

        assertEquals(DownloadInterstitialProperties.State.PAUSED, mModel.get(STATE));
        assertEquals(OfflineItemState.PAUSED, mModel.get(DOWNLOAD_ITEM).state);
        mUmaTestingHelper.assertActionWasLogged(PAUSED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testInProgressResumeDownload() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mModel.get(ListProperties.CALLBACK_PAUSE).onResult(mModel.get(DOWNLOAD_ITEM));
        clickButtonWithText(RESUME_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.IN_PROGRESS, mModel.get(STATE));
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        mUmaTestingHelper.assertActionWasLogged(PAUSED, 1);
        mUmaTestingHelper.assertActionWasLogged(RESUMED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testCancelledDownloadIsDeletedImmediately() {
        assertEquals(OfflineItemState.IN_PROGRESS, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(CANCEL_BUTTON_TEXT);

        assertFalse(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
        mUmaTestingHelper.assertActionWasLogged(CANCELLED, 1);
        mUmaTestingHelper.assertActionWasLogged(DELETED, 0);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testOpenDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(OPEN_BUTTON_TEXT);

        assertEquals(mModel.get(DOWNLOAD_ITEM).id, mProvider.mLastOpenedDownload);
        mUmaTestingHelper.assertActionWasLogged(OPENED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testDeleteDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        mModalDialogManager.clickPositiveButton();

        assertTrue(mSnackbarShown);
        assertEquals(DownloadInterstitialProperties.State.CANCELLED, mModel.get(STATE));
        mUmaTestingHelper.assertActionWasLogged(COMPLETED, 1);
        mUmaTestingHelper.assertActionWasLogged(DELETED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testCancelDeleteDialogKeepsDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        mModalDialogManager.clickNegativeButton();

        assertEquals(DownloadInterstitialProperties.State.SUCCESSFUL, mModel.get(STATE));
        mUmaTestingHelper.assertActionWasLogged(COMPLETED, 1);
        mUmaTestingHelper.assertActionWasLogged(DELETED, 0);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testReDownloadDeletedDownload() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        mModalDialogManager.clickPositiveButton();
        clickButtonWithText(DOWNLOAD_BUTTON_TEXT);

        assertEquals(DownloadInterstitialProperties.State.PENDING, mModel.get(STATE));
        // Increment the reloaded item to move to in progress state.
        mProvider.incrementProgress(new ContentId("test", "reloaded-item"));
        assertEquals(DownloadInterstitialProperties.State.IN_PROGRESS, mModel.get(STATE));

        assertEquals(1, mProvider.getItems().size());
        assertTrue(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
        mUmaTestingHelper.assertActionWasLogged(COMPLETED, 1);
        mUmaTestingHelper.assertActionWasLogged(DELETED, 1);
        mUmaTestingHelper.assertActionWasLogged(REINITIATED, 1);
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testDeletedDownloadIsRemovedImmediately() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        assertEquals(OfflineItemState.COMPLETE, mModel.get(DOWNLOAD_ITEM).state);
        clickButtonWithText(DELETE_BUTTON_TEXT);
        mModalDialogManager.clickPositiveButton();

        assertFalse(mProvider.getItems().contains(mModel.get(DOWNLOAD_ITEM)));
    }

    @Test
    @SmallTest
    @Feature({"NewDownloadTab"})
    public void testSharingLogsMetrics() {
        mProvider.completeDownload(mModel.get(DOWNLOAD_ITEM).id);
        mModel.get(ListProperties.CALLBACK_SHARE).onResult(mModel.get(DOWNLOAD_ITEM));
        mUmaTestingHelper.assertActionWasLogged(SHARED, 1);
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

    private void reloadTab() {
        OfflineItem item = createOfflineItem("reloaded-item");
        mProvider.addItem(item);
        mProvider.incrementProgress(item.id);
    }

    private static OfflineItem createOfflineItem(String id) {
        OfflineItem item = new OfflineItem();
        item.id = new ContentId("test", id);
        item.progress = new OfflineItem.Progress(0L, 10L, OfflineItemProgressUnit.BYTES);
        item.state = OfflineItemState.IN_PROGRESS;
        item.title = "Test Item";
        item.description = "Test Description";
        item.originalUrl = JUnitTestGURLs.URL_2;
        return item;
    }

    /**
     * Helper class which provides utility methods for retrieving the number of logged UMA metrics
     * for different UI actions. The live number of logs can be compared with the initial number of
     * logs to test how many metrics of each type were logged during the test.
     */
    private static class UmaTestingHelper {
        private Map<Integer, Integer> mValues;

        UmaTestingHelper() {
            mValues = new HashMap<Integer, Integer>();
        }

        /**
         * Captures the number of each metric logged at a given moment. Should be called during test
         * setup.
         */
        void initialise() {
            mValues.put(INITIATED, getValueCount(INITIATED));
            mValues.put(COMPLETED, getValueCount(COMPLETED));
            mValues.put(CANCELLED, getValueCount(CANCELLED));
            mValues.put(PAUSED, getValueCount(PAUSED));
            mValues.put(RESUMED, getValueCount(RESUMED));
            mValues.put(OPENED, getValueCount(OPENED));
            mValues.put(DELETED, getValueCount(DELETED));
            mValues.put(REINITIATED, getValueCount(REINITIATED));
            mValues.put(SHARED, getValueCount(SHARED));
            mValues.put(RENAMED, getValueCount(RENAMED));
        }

        /**
         * Asserts that an action was logged a certain number of times.
         *
         * @param action The action that is being queried.
         * @param numberOfTimes The expected number of times the action has been logged.
         */
        void assertActionWasLogged(
                @DownloadInterstitialMediator.UmaHelper.Action int action, int numberOfTimes) {
            assertEquals(numberOfTimes, getValueCount(action) - getInitialValueCount(action));
        }

        /**
         * Returns the log count as of when {@link UmaTestingHelper#initialise()} was called for a
         * given action.
         */
        private int getInitialValueCount(
                @DownloadInterstitialMediator.UmaHelper.Action int action) {
            return mValues.get(action);
        }

        /** Returns the live log count for a given action */
        private int getValueCount(@DownloadInterstitialMediator.UmaHelper.Action int action) {
            return RecordHistogram.getHistogramValueCountForTesting(
                    "Download.Interstitial.UIAction", action);
        }
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
            item.progress =
                    new OfflineItem.Progress(
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
            item.progress =
                    new OfflineItem.Progress(
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
        public void resumeDownload(ContentId id) {
            if (findItem(id).state != OfflineItemState.COMPLETE) {
                findItem(id).state = OfflineItemState.IN_PROGRESS;
            }
            notifyObservers(id);
        }

        @Override
        public void cancelDownload(ContentId id) {
            findItem(id).state = OfflineItemState.CANCELLED;
            removeItem(id);
            notifyObserversOfRemoval(id);
        }
    }
}
