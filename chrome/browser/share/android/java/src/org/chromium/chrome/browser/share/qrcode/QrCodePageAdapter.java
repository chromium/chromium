// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.view.View;
import android.view.ViewGroup;

import androidx.viewpager.widget.PagerAdapter;

import java.util.ArrayList;

/**
 * QrCodePageAdapter instantiates and destroys provided tab views.
 */
class QrCodePageAdapter extends PagerAdapter {
    ArrayList<View> mPages;

    public QrCodePageAdapter(ArrayList<View> pages) {
        mPages = pages;
    }

    // PagerAdapter implementation.
    @Override
    public Object instantiateItem(ViewGroup parent, int position) {
        View page = mPages.get(position);
        parent.addView(page);

        return page;
    }

    @Override
    public void destroyItem(ViewGroup parent, int position, Object view) {
        parent.removeView((View) view);
    }

    @Override
    public int getCount() {
        return mPages.size();
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view == object;
    }
}
