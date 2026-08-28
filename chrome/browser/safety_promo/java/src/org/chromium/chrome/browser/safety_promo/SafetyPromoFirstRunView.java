// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/** View for the Safety Promo during the First Run Experience (FRE). */
@NullMarked
public class SafetyPromoFirstRunView extends RelativeLayout {
    private ButtonCompat mContinueButton;
    private LinearLayout mCardsContainer;

    public SafetyPromoFirstRunView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContinueButton = findViewById(R.id.fre_continue_button);
        mCardsContainer = findViewById(R.id.safety_promo_cards_container);
    }

    public ButtonCompat getContinueButtonView() {
        assert mContinueButton != null;
        return mContinueButton;
    }

    public void setCards(List<SafetyPromoItem> items) {
        assert mCardsContainer != null;
        mCardsContainer.removeAllViews();
        LayoutInflater inflater = LayoutInflater.from(getContext());
        for (SafetyPromoItem item : items) {
            View cardView =
                    inflater.inflate(R.layout.safety_promo_card_item, mCardsContainer, false);
            bindCard(cardView, item);
            mCardsContainer.addView(cardView);
        }
    }

    private static void bindCard(View cardView, SafetyPromoItem item) {
        ImageView iconView = cardView.findViewById(R.id.card_icon);
        TextView titleView = cardView.findViewById(R.id.card_title);
        TextView subtitleView = cardView.findViewById(R.id.card_subtitle);

        iconView.setImageResource(item.cardIconResId);
        titleView.setText(item.cardTitleResId);
        subtitleView.setText(item.cardSubtitleResId);
    }
}
