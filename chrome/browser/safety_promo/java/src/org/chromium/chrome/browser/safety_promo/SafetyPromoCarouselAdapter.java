// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** RecyclerView Adapter to display illustrations in the Safety FRE promo carousel. */
@NullMarked
public class SafetyPromoCarouselAdapter
        extends RecyclerView.Adapter<SafetyPromoCarouselAdapter.ViewHolder> {

    private final List<SafetyPromoItem> mItems;

    public SafetyPromoCarouselAdapter(List<SafetyPromoItem> items) {
        mItems = items;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.safety_promo_carousel_illustration, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        SafetyPromoItem item = mItems.get(position);
        holder.mImageView.setImageResource(item.carouselIllustrationResId);
    }

    @Override
    public int getItemCount() {
        return mItems.size();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        final ImageView mImageView;

        ViewHolder(View itemView) {
            super(itemView);
            mImageView = itemView.findViewById(R.id.carousel_illustration);
        }
    }
}
