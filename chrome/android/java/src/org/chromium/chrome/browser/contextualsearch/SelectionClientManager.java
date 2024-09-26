// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Build;
import android.view.textclassifier.TextClassifier;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionEventProcessor;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.touch_selection.SelectionEventType;

/**
 * Manages the current {@link SelectionClient} instances, with support for 0-2 instances.
 * This class supports one permanent instance for Smart Text Selection, and one non-permanent
 * instance for Contextual Search that can be added or removed. <p> Usage: After being constructed
 * this class knows if Smart Select is active or not, and can return a {@link SelectionClient}.
 * If Smart Select is active it will return the Smart Select Client from
 * {@link #getSelectionClient}, and if not then {@link #getSelectionClient()} will return {@code
 * null}. A non-permanent client may be added using
 * {@link #addContextualSearchSelectionClient(SelectionClient)} to connect to Contextual Search.
 * This client may be removed later using {@link #removeContextualSearchSelectionClient()}.
 */
public class SelectionClientManager {
    // Whether Smart Select is allowed to be enabled in Chrome.
    private final boolean mIsSmartSelectionEnabledInChrome;

    /**
     * The single optional client supported directly by this class.
     * It may be null, the Smart Selection client, or our bridge between Smart Select and Contextual
     * Search.
     */
    private @Nullable SelectionClient mOptionalSelectionClient;

    /**
     * Constructs an instance that can return a {@link SelectionClient} that's a mix of an optional
     * Smart Selection client and a transient Contextual Search client.
     * @param webContents The {@link WebContents} that will show popups for this client.
     */
    SelectionClientManager(WebContents webContents) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.O) {
            assert webContents != null;
            mOptionalSelectionClient = SelectionClient.createSmartSelectionClient(webContents);
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(webContents);
            controller.setSelectionClient(mOptionalSelectionClient);
        }
        mIsSmartSelectionEnabledInChrome = mOptionalSelectionClient != null;
    }

    /** Test-only constructor. */
    @VisibleForTesting
    SelectionClientManager(SelectionClient optionalSelectionClient, boolean enableSmartSelection) {
        mOptionalSelectionClient = optionalSelectionClient;
        mIsSmartSelectionEnabledInChrome = enableSmartSelection;
    }

    /**
     * @return the current {@link SelectionClient} or {@code null} if there is none currently
     *         active.
     */
    @Nullable
    SelectionClient getSelectionClient() {
        return mOptionalSelectionClient;
    }

    /**
     * Adds the given {@link SelectionClient} as an additional temporary instance that will be
     * notified of method calls.
     * @param contextualSearchSelectionClient An additional {@link SelectionClient} that should be
     *        notified of requests going forward, used by Contextual Search.
     * @return The resulting {@link SelectionClient} that should be active after the addition.
     */
    SelectionClient addContextualSearchSelectionClient(
            SelectionClient contextualSearchSelectionClient) {
        assert contextualSearchSelectionClient != null;
        assert !(mOptionalSelectionClient instanceof SelectionClientBridge)
                : "No more than two selection client instances are supported!";
        if (mIsSmartSelectionEnabledInChrome) {
            mOptionalSelectionClient =
                    new SelectionClientBridge(
                            mOptionalSelectionClient, contextualSearchSelectionClient);
        } else {
            mOptionalSelectionClient = contextualSearchSelectionClient;
        }
        return mOptionalSelectionClient;
    }

    /**
     * Removes the current {@link SelectionClient} from the current instances that will be notified
     * of method calls.
     * @return A remaining {@link SelectionClient} used for Smart Selection or {@code null}.
     */
    @Nullable
    SelectionClient removeContextualSearchSelectionClient() {
        if (mIsSmartSelectionEnabledInChrome) {
            assert mOptionalSelectionClient instanceof SelectionClientBridge
                    : "Looks like it was never added.";
            SelectionClientBridge currentSelectionClientBridge =
                    (SelectionClientBridge) mOptionalSelectionClient;
            mOptionalSelectionClient = currentSelectionClientBridge.getSmartSelectionClient();
        } else {
            assert !(mOptionalSelectionClient instanceof SelectionClientBridge)
                    : "Internal error managing selection clients.";
            mOptionalSelectionClient = null;
        }
        return mOptionalSelectionClient;
    }

    /**
     * Bridges exactly two {@link SelectionClient} instances.  Both sides of the bridge receive
     * calls for most incoming calls to this instance.
     */
    private static class SelectionClientBridge implements SelectionClient {
        private final SelectionClient mSmartSelectionClient;
        private final SelectionClient mContextualSearchSelectionClient;

        /**
         * Constructs a bridge between the {@code smartSelectionClient} and the given
         * {@code contextualSearchSelectionClient}.  Method calls to this class are repeated to both
         * instances, with some exceptions: calls that return a value are only routed to the
         * {@code smartSelectionClient}.
         * @param smartSelectionClient The platform-dependent {@link SelectionClient}, which will be
         *        used as the dominant client, responsible for any results that need to be returned
         *        from method calls (typically a {@code SmartSelectionClient}).
         * @param contextualSearchSelectionClient A {@link SelectionClient} based on the
         *        {@code ContextualSearchManager}.
         */
        private SelectionClientBridge(
                SelectionClient smartSelectionClient,
                SelectionClient contextualSearchSelectionClient) {
            mSmartSelectionClient = smartSelectionClient;
            mContextualSearchSelectionClient = contextualSearchSelectionClient;
        }

        /**
         * @return The current Smart Selection client.
         */
        private SelectionClient getSmartSelectionClient() {
            return mSmartSelectionClient;
        }

        @Override
        public void onSelectionChanged(String selection) {
            mSmartSelectionClient.onSelectionChanged(selection);
            mContextualSearchSelectionClient.onSelectionChanged(selection);
        }

        @Override
        public void onSelectionEvent(
                @SelectionEventType int eventType, float posXPix, float posYPix) {
            mSmartSelectionClient.onSelectionEvent(eventType, posXPix, posYPix);
            mContextualSearchSelectionClient.onSelectionEvent(eventType, posXPix, posYPix);
        }

        @Override
        public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {
            mSmartSelectionClient.selectAroundCaretAck(result);
            mContextualSearchSelectionClient.selectAroundCaretAck(result);
        }

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return mSmartSelectionClient.requestSelectionPopupUpdates(shouldSuggest);
        }

        @Override
        public void cancelAllRequests() {
            mSmartSelectionClient.cancelAllRequests();
            mContextualSearchSelectionClient.cancelAllRequests();
        }

        @Override
        public void setTextClassifier(TextClassifier textClassifier) {
            mSmartSelectionClient.setTextClassifier(textClassifier);
            mContextualSearchSelectionClient.setTextClassifier(textClassifier);
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
    }
}
