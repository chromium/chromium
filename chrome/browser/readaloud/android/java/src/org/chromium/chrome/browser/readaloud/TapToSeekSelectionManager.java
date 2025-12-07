// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.textclassifier.TextClassifier;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionEventProcessor;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.touch_selection.SelectionEventType;

/**
 * Manages hooking and unhooking TapToSeekSelectionClient into the active playback tab. Listens for
 * when SmartSelection has the surrounding text for a selection to send it to ReadAloud's tap to
 * seek feature.
 */
@NullMarked
public class TapToSeekSelectionManager implements SelectionClient.SurroundingTextCallback {
    // Tab that Tap to Seek is hooked into. Can be null if not hooked into any tab.
    private @Nullable Tab mObservingTab;
    private final ReadAloudController mReadAloudController;

    /**
     * The single optional client supported directly by this class. The bridge between Smart Select
     * and Tap to Seek.
     */
    private @Nullable TapToSeekSelectionClient mSelectionClient;

    private static @Nullable SelectionClient sSmartSelectionClientForTesting;
    private static @Nullable SelectionPopupController sSelectionPopupControllerForTesting;

    TapToSeekSelectionManager(
            ReadAloudController readAloudController,
            ObservableSupplier<@Nullable Tab> activePlaybackTab) {
        mReadAloudController = readAloudController;
        activePlaybackTab.addObserver(this::onActivePlaybackTabUpdated);
    }

    @Override
    public void onSurroundingTextReceived(String text, int start, int end) {
        // On taps, the start and end index are the same. We want to filter out selection events.
        if (start == end) {
            mReadAloudController.tapToSeek(text, start, end);
        }
    }

    @VisibleForTesting
    public void onActivePlaybackTabUpdated(@Nullable Tab tab) {
        updateHooksForTab(tab);
        mObservingTab = tab;
    }

    private void updateHooksForTab(@Nullable Tab tab) {
        boolean isWebcontentsSame =
                tab != null
                        && mObservingTab != null
                        && tab.getWebContents() == mObservingTab.getWebContents();
        if (!isWebcontentsSame) {
            if (mObservingTab != null) {
                removeHooks(mObservingTab.getWebContents());
            }
            if (tab == null) return;
            WebContents currentWebContents = tab.getWebContents();
            if (currentWebContents != null) {
                addHooks(currentWebContents);
            }
        }
    }

    private void addHooks(@Nullable WebContents webContents) {
        if (webContents != null) {
            mSelectionClient =
                    new TapToSeekSelectionClient(
                            assumeNonNull(
                                    sSmartSelectionClientForTesting != null
                                            ? sSmartSelectionClientForTesting
                                            : SelectionClient.createSmartSelectionClient(
                                                    webContents)));
            mSelectionClient.addSurroundingTextReceivedListeners(this);
            SelectionPopupController controller =
                    sSelectionPopupControllerForTesting != null
                            ? sSelectionPopupControllerForTesting
                            : SelectionPopupController.fromWebContents(webContents);
            controller.setSelectionClient(mSelectionClient);
        }
    }

    private void removeHooks(@Nullable WebContents webContents) {
        if (webContents != null) {
            SelectionPopupController controller =
                    sSelectionPopupControllerForTesting != null
                            ? sSelectionPopupControllerForTesting
                            : SelectionPopupController.fromWebContents(webContents);
            assumeNonNull(mSelectionClient);
            mSelectionClient.removeSurroundingTextReceivedListeners(this);
            if (controller.getSelectionClient() == mSelectionClient) {
                controller.setSelectionClient(null);
            }
            mSelectionClient = null;
            mObservingTab = null;
        }
    }

    /**
     * Sends selection events to the smart selection client. Modified to call
     * requestSelectionPopupUpdates in onSelectionChanged to gather surrounding text after taps.
     */
    @VisibleForTesting
    public static class TapToSeekSelectionClient implements SelectionClient {
        private final SelectionClient mSmartSelectionClient;

        /**
         * @param webContents WebContents to get the smart selection client.
         */
        private TapToSeekSelectionClient(SelectionClient selectionClient) {
            mSmartSelectionClient = selectionClient;
        }

        @Override
        public void onSelectionChanged(String selection) {
            mSmartSelectionClient.onSelectionChanged(selection);
            requestSelectionPopupUpdates(false);
        }

        @Override
        public void onSelectionEvent(
                @SelectionEventType int eventType, float posXPix, float posYPix) {
            mSmartSelectionClient.onSelectionEvent(eventType, posXPix, posYPix);
        }

        @Override
        public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {
            mSmartSelectionClient.selectAroundCaretAck(result);
        }

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return mSmartSelectionClient.requestSelectionPopupUpdates(shouldSuggest);
        }

        @Override
        public void cancelAllRequests() {
            mSmartSelectionClient.cancelAllRequests();
        }

        @Override
        public void setTextClassifier(TextClassifier textClassifier) {
            mSmartSelectionClient.setTextClassifier(textClassifier);
        }

        @Override
        public TextClassifier getTextClassifier() {
            return assumeNonNull(mSmartSelectionClient.getTextClassifier());
        }

        @Override
        public TextClassifier getCustomTextClassifier() {
            return assumeNonNull(mSmartSelectionClient.getCustomTextClassifier());
        }

        @Override
        public SelectionEventProcessor getSelectionEventProcessor() {
            return assumeNonNull(mSmartSelectionClient.getSelectionEventProcessor());
        }

        @Override
        public void addSurroundingTextReceivedListeners(SurroundingTextCallback observer) {
            mSmartSelectionClient.addSurroundingTextReceivedListeners(observer);
        }

        @Override
        public void removeSurroundingTextReceivedListeners(SurroundingTextCallback observer) {
            mSmartSelectionClient.removeSurroundingTextReceivedListeners(observer);
        }
    }

    @VisibleForTesting
    @Nullable
    public TapToSeekSelectionClient getSelectionClient() {
        return mSelectionClient;
    }

    @VisibleForTesting
    public static void setSmartSelectionClient(SelectionClient selectionClient) {
        sSmartSelectionClientForTesting = selectionClient;
        ResettersForTesting.register(() -> sSmartSelectionClientForTesting = null);
    }

    @VisibleForTesting
    public static void setSelectionPopupController(
            SelectionPopupController selectionPopupController) {
        sSelectionPopupControllerForTesting = selectionPopupController;
        ResettersForTesting.register(() -> sSelectionPopupControllerForTesting = null);
    }
}
