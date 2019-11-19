// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.view.ViewStub;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feed.FeedConfiguration;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.text.SpanApplier;

/**
 * Shows a card prompting the user to view offline content when they are offline. This class is
 * responsible for inflating the card layout and displaying the card based on network connectivity.
 */
public class ExploreOfflineCard {
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

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_HOME)) return;
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
        boolean isVisible = DownloadUtils.shouldShowOfflineHome();
        View cardView = mRootView.findViewById(R.id.explore_offline_card);

        if (cardView == null) {
            if (!isVisible) return;
            cardView = inflateCardLayout();
        }
        cardView.setVisibility(DownloadUtils.shouldShowOfflineHome() ? View.VISIBLE : View.GONE);
    }

    private View inflateCardLayout() {
        View cardView =
                ((ViewStub) mRootView.findViewById(R.id.explore_offline_card_stub)).inflate();
        TextView messageView = cardView.findViewById(R.id.explore_offline_text);
        messageView.setText(SpanApplier.applySpans(cardView.getContext().getResources().getString(
                                                           R.string.explore_offline_card_message),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new ForegroundColorSpan(
                                ApiCompatibilityUtils.getColor(cardView.getContext().getResources(),
                                        R.color.blue_when_enabled)))));

        View imageView = cardView.findViewById(R.id.explore_offline_image);
        imageView.setBackground(imageView.getContext().getResources().getDrawable(
                FeedConfiguration.getFeedUiEnabled()
                        ? R.drawable.card_background_rounded_right_half_with_border
                        : R.drawable.card_background_rounded_right_half_no_border));

        cardView.setOnClickListener(v -> mOpenDownloadHomeCallback.run());
        return cardView;
    }
}
