// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud;

import android.os.Build;
import android.view.textclassifier.TextClassifier;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
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
public class TapToSeekSelectionManager implements SelectionClient.SurroundingTextCallback {
    // Whether Smart Select is allowed to be enabled in Chrome. Set to true for Android O+. This
    // check is also mirrored in {@link SelectionClientManager} when making the Smart Selection
    // Client
    private static final boolean IS_SMART_SELECTION_ENABLED_IN_CHROME =
            Build.VERSION.SDK_INT > Build.VERSION_CODES.O;
    // Tab that Tap to Seek is hooked into. Can be null if not hooked into any tab.
    @Nullable Tab mObservingTab;
    private final ReadAloudController mReadAloudController;

    /**
     * The single optional client supported directly by this class. The bridge between Smart Select
     * and Tap to Seek.
     */
    private @Nullable TapToSeekSelectionClient mSelectionClient;

    @Nullable private static SelectionClient sSmartSelectionClientForTesting;
    @Nullable private static SelectionPopupController sSelectionPopupControllerForTesting;

    TapToSeekSelectionManager(
            ReadAloudController readAloudController, ObservableSupplier<Tab> activePlaybackTab) {
        mReadAloudController = readAloudController;
        if (IS_SMART_SELECTION_ENABLED_IN_CHROME) {
            activePlaybackTab.addObserver(this::onActivePlaybackTabUpdated);
        }
    }

    @Override
    public void onSurroundingTextReceived(String text, int start, int end) {
        // On taps, the start and end index are the same. We want to filter out selection events.
        if (start == end) {
            mReadAloudController.tapToSeek(text, start, end);
        }
    }

    @VisibleForTesting
    public void onActivePlaybackTabUpdated(Tab tab) {
        updateHooksForTab(tab);
        mObservingTab = tab;
    }

    private void updateHooksForTab(Tab tab) {
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

    private void addHooks(WebContents webContents) {
        if (IS_SMART_SELECTION_ENABLED_IN_CHROME && webContents != null) {
            mSelectionClient =
                    new TapToSeekSelectionClient(
                            sSmartSelectionClientForTesting != null
                                    ? sSmartSelectionClientForTesting
                                    : SelectionClient.createSmartSelectionClient(webContents));
            mSelectionClient.addSurroundingTextReceivedListeners(this);
            SelectionPopupController controller =
                    sSelectionPopupControllerForTesting != null
                            ? sSelectionPopupControllerForTesting
                            : SelectionPopupController.fromWebContents(webContents);
            controller.setSelectionClient(mSelectionClient);
        }
    }

    private void removeHooks(WebContents webContents) {
        if (webContents != null) {
            SelectionPopupController controller =
                    sSelectionPopupControllerForTesting != null
                            ? sSelectionPopupControllerForTesting
                            : SelectionPopupController.fromWebContents(webContents);
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
            return mSmartSelectionClient.getTextClassifier();
        }

        @Override
        public TextClassifier getCustomTextClassifier() {
            return mSmartSelectionClient.getCustomTextClassifier();
        }

        @Override
        public SelectionEventProcessor getSelectionEventProcessor() {
            return mSmartSelectionClient.getSelectionEventProcessor();
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
