// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Controls the behavior of the ViewPager to navigate between privacy review steps. */
public class PrivacyReviewPagerAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    /**
     * The types of views supported. Each view corresponds to a step in the privacy review.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.COOKIES, ViewType.MSBB, ViewType.SAFE_BROWSING, ViewType.SYNC,
            ViewType.COUNT})
    private @interface ViewType {
        int COOKIES = 0;
        int MSBB = 1;
        int SAFE_BROWSING = 2;
        int SYNC = 3;
        int COUNT = 4;
    }

    class CookiesViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public CookiesViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    class MSBBViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public MSBBViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    class SafeBrowsingViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public SafeBrowsingViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    class SyncViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public SyncViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    @Override
    public int getItemViewType(int position) {
        // Each view is unique, so return the position directly, instead of 0 by default.
        if (position == 0) return ViewType.MSBB;
        if (position == 1) return ViewType.SYNC;
        if (position == 2) return ViewType.SAFE_BROWSING;
        return ViewType.COOKIES;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());
        switch (viewType) {
            case ViewType.MSBB:
                return new MSBBViewHolder(
                        inflater.inflate(R.layout.privacy_review_msbb_step, parent, false));
            case ViewType.SYNC:
                return new SyncViewHolder(
                        inflater.inflate(R.layout.privacy_review_sync_step, parent, false));
            case ViewType.SAFE_BROWSING:
                return new SafeBrowsingViewHolder(
                        inflater.inflate(R.layout.privacy_review_sb_step, parent, false));
            case ViewType.COOKIES:
                return new CookiesViewHolder(
                        inflater.inflate(R.layout.privacy_review_cookies_step, parent, false));
        }
        return null;
    }

    @Override
    public int getItemCount() {
        return ViewType.COUNT;
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        return;
    }
}
