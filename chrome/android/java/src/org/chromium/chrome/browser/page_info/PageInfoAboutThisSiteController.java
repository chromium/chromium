// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/**
 * Class for controlling the page info 'About This Site' section.
 */
public class PageInfoAboutThisSiteController implements PageInfoSubpageController {
    public static final int ROW_ID = View.generateViewId();

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mSiteDescription;

    public PageInfoAboutThisSiteController(PageInfoMainController mainController,
            PageInfoRowView rowView, PageInfoControllerDelegate delegate) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mSiteDescription = getSiteDescription();
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
        TextView view = new TextView(parent.getContext());
        view.setText(mSiteDescription);
        return view;
    }

    @Override
    public void onSubpageRemoved() {}

    private void setupRow() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        String subtitle = null;
        if (mSiteDescription != null && !mSiteDescription.isEmpty()) {
            subtitle = mSiteDescription;
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

    private String getSiteDescription() {
        return PageInfoAboutThisSiteControllerJni.get().getSiteDescription(
                mMainController.getBrowserContext(), mMainController.getURL());
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @NativeMethods
    interface Natives {
        // TODO(crbug.com/1250653): Pass protobuf instead.
        String getSiteDescription(BrowserContextHandle browserContext, GURL url);
    }
}
