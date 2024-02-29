// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
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

    private boolean mIsSuggestionBundleReady;
    private String mTitle;
    private String mAllTilesTexts;

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

    void setUrlImageProvider(UrlImageProvider urlImageProvider) {
        mUrlImageProvider = urlImageProvider;
        renderIfReady();
    }

    void setClickCallback(SuggestionClickCallback clickCallback) {
        mClickCallback = clickCallback;
        renderIfReady();
    }

    /** Assumes `mUrlImageProvider` and `mClickCallback` are assigned, triggers render. */
    void setSuggestionBundle(SuggestionBundle bundle) {
        mIsSuggestionBundleReady = true;
        mBundle = bundle;
        renderIfReady();
    }

    void setTitle(@Nullable String title) {
        mTitle = title;
        ((TextView) findViewById(R.id.tab_resumption_title_description)).setText(mTitle);
        setContentDescriptionOfTabResumption();
    }

    TabResumptionTileContainerView getTileContainerViewForTesting() {
        return mTileContainerView;
    }

    private void renderIfReady() {
        if (mIsSuggestionBundleReady && mUrlImageProvider != null && mClickCallback != null) {
            if (mBundle == null) {
                mTileContainerView.removeAllViews();
                mAllTilesTexts = null;
            } else {
                mAllTilesTexts =
                        mTileContainerView.renderAllTiles(
                                mBundle, mUrlImageProvider, mClickCallback);
            }
            setContentDescriptionOfTabResumption();
        }
    }

    /** Sets the content description for the tab resumption module. */
    private void setContentDescriptionOfTabResumption() {
        if (mTitle != null && mAllTilesTexts != null) {
            setContentDescription(mTitle + ". " + mAllTilesTexts);
        } else {
            setContentDescription(null);
        }
    }
}
