// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/**
 * Class for controlling the page info 'About This Site' section.
 */
public class PageInfoAboutThisSiteController implements PageInfoSubpageController {
    public static final int ROW_ID = View.generateViewId();
    private static final String TAG = "PageInfo";

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final @Nullable SiteInfo mSiteInfo;

    public PageInfoAboutThisSiteController(PageInfoMainController mainController,
            PageInfoRowView rowView, PageInfoControllerDelegate delegate) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mSiteInfo = getSiteInfo();
        setupRow();
    }

    private void launchSubpage() {
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        // TODO(crbug.com/1250653): Add translated string.
        return "About this site";
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        // The subpage can only be created if there is a row and the row is only visible if siteInfo
        // is populated.
        assert mSiteInfo != null;
        assert mSiteInfo.hasDescription();
        TextView view = new TextView(parent.getContext());
        view.setText(mSiteInfo.getDescription().getDescription());
        return view;
    }

    @Override
    public void onSubpageRemoved() {}

    private void setupRow() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        String subtitle = null;
        if (mSiteInfo != null && mSiteInfo.hasDescription()) {
            subtitle = mSiteInfo.getDescription().getDescription();
        }

        // TODO(crbug.com/1250653): Add translated string.
        rowParams.title = "Site info";
        rowParams.subtitle = subtitle;
        rowParams.visible =
                subtitle != null && mDelegate.isSiteSettingsAvailable() && !mDelegate.isIncognito();
        rowParams.iconResId = R.drawable.ic_info_outline_grey_24dp;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;

        mRowView.setParams(rowParams);
    }

    private @Nullable SiteInfo getSiteInfo() {
        byte[] result = PageInfoAboutThisSiteControllerJni.get().getSiteInfo(
                mMainController.getBrowserContext(), mMainController.getURL());
        if (result == null) return null;
        SiteInfo info = null;
        try {
            info = SiteInfo.parseFrom(result);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            Log.e(TAG, "Could not parse proto: %s", e);
            assert false;
        }
        return info;
    }
    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @NativeMethods
    interface Natives {
        byte[] getSiteInfo(BrowserContextHandle browserContext, GURL url);
    }
}
