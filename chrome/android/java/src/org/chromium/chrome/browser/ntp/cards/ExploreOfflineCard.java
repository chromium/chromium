// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.View;
import android.view.ViewStub;
import android.widget.Button;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.ExploreOfflineStatusProvider;
import org.chromium.net.NetworkChangeNotifier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Shows a card prompting the user to view offline content when they are offline. This class is
 * responsible for inflating the card layout and displaying the card based on network connectivity.
 */
public class ExploreOfflineCard {
    // Please treat this list as append only and keep it in sync with
    // NewTabPage.ExploreOffline.Action in enums.xml.
    @IntDef({Action.SHOWN, Action.CONFIRM, Action.CANCEL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Action {
        int SHOWN = 0;
        int CONFIRM = 1;
        int CANCEL = 2;
        int NUM_ENTRIES = 3;
    }

    private static boolean sCardDismissed;
    private final View mRootView;
    private final Runnable mOpenDownloadHomeCallback;
    private NetworkChangeNotifier.ConnectionTypeObserver mConnectionTypeObserver;

    /**
     * Constructor.
     * @param rootView The parent view where this card should be inflated.
     * @param openDownloadHomeCallback A callback to open download home.
     */
    public ExploreOfflineCard(View rootView, Runnable openDownloadHomeCallback) {
        mRootView = rootView;
        mOpenDownloadHomeCallback = openDownloadHomeCallback;

        setCardViewVisibility();
        mConnectionTypeObserver = connectionType -> {
            setCardViewVisibility();
        };

        NetworkChangeNotifier.addConnectionTypeObserver(mConnectionTypeObserver);
    }

    /** Called during destruction of the view. */
    public void destroy() {
        NetworkChangeNotifier.removeConnectionTypeObserver(mConnectionTypeObserver);
    }

    private void setCardViewVisibility() {
        boolean isVisible = !sCardDismissed && shouldShowExploreOfflineMessage();
        View cardView = mRootView.findViewById(R.id.explore_offline_card);

        if (cardView == null) {
            if (!isVisible) return;
            cardView = inflateCardLayout();
        }
        cardView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private View inflateCardLayout() {
        View cardView =
                ((ViewStub) mRootView.findViewById(R.id.explore_offline_card_stub)).inflate();
        Button confirmButton = cardView.findViewById(R.id.button_primary);
        Button cancelButton = cardView.findViewById(R.id.button_secondary);

        confirmButton.setOnClickListener(v -> {
            sCardDismissed = true;
            setCardViewVisibility();
            mOpenDownloadHomeCallback.run();
            recordStats(Action.CONFIRM);
        });
        cancelButton.setOnClickListener(v -> {
            sCardDismissed = true;
            setCardViewVisibility();
            recordStats(Action.CANCEL);
        });

        recordStats(Action.SHOWN);
        return cardView;
    }

    private void recordStats(@Action int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.ExploreOffline.Action", action, Action.NUM_ENTRIES);
    }

    private static boolean shouldShowExploreOfflineMessage() {
        return NetworkChangeNotifier.isInitialized() && !NetworkChangeNotifier.isOnline()
                && ExploreOfflineStatusProvider.getInstance().isPrefetchContentAvailable();
    }
}
