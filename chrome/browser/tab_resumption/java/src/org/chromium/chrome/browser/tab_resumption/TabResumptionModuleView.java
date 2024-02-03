// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;

/**
 * The View for the tab resumption module, consisting of a header followed by suggestion tile(s).
 */
public class TabResumptionModuleView extends LinearLayout {
    private TabResumptionTileContainerView mTileContainerView;
    private UrlImageProvider mUrlImageProvider;
    private SuggestionClickCallback mClickCallback;
    private SuggestionBundle mBundle;

    public TabResumptionModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTileContainerView = findViewById(R.id.tab_resumption_module_tiles_container);
    }

    void destroy() {
        mTileContainerView.destroy();
    }

    void updateTitle(int numTiles) {
        Resources res = getResources();
        String title = res.getQuantityString(R.plurals.home_modules_tab_resumption_title, numTiles);
        ((TextView) findViewById(R.id.tab_resumption_title_description)).setText(title);
    }

    void setUrlImageProvider(UrlImageProvider urlImageProvider) {
        mUrlImageProvider = urlImageProvider;
    }

    void setClickCallback(SuggestionClickCallback clickCallback) {
        mClickCallback = clickCallback;
    }

    /** Assumes `mUrlImageProvider` and `mClickCallback` are assigned, triggers render. */
    void setSuggestionBundleThenRender(SuggestionBundle bundle) {
        mBundle = bundle;
        if (mBundle == null) {
            mTileContainerView.removeAllViews();
        } else {
            assert mUrlImageProvider != null;
            assert mClickCallback != null;
            mTileContainerView.renderAllTiles(mBundle, mUrlImageProvider, mClickCallback);
            updateTitle(mBundle.entries.size());
        }
    }

    TabResumptionTileContainerView getTileContainerViewForTesting() {
        return mTileContainerView;
    }
}
