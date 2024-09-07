// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.res.Resources;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabObserver;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabSheetContent;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/** Class for controlling the page info 'About This Site' section. */
public class PageInfoAboutThisSiteController {
    public static final int ROW_ID = View.generateViewId();
    private static final String TAG = "PageInfo";

    private final PageInfoMainController mMainController;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final WebContents mWebContents;
    private @Nullable SiteInfo mSiteInfo;
    private EphemeralTabCoordinator mEphemeralTabCoordinator;
    private EphemeralTabObserver mEphemeralTabObserver;
    private final TabCreator mTabCreator;

    static boolean isFeatureEnabled() {
        return PageInfoAboutThisSiteControllerJni.get().isFeatureEnabled();
    }

    public PageInfoAboutThisSiteController(
            PageInfoMainController mainController,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate,
            WebContents webContents,
            TabCreator tabCreator) {
        mMainController = mainController;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mRowView = rowView;
        mDelegate = delegate;
        mWebContents = webContents;
        mTabCreator = tabCreator;
        setupRow();
    }

    private void openUrl(String url, @PageInfoAction int action) {
        mMainController.recordAction(action);
        mEphemeralTabCoordinator =
                mEphemeralTabCoordinatorSupplier != null
                        ? mEphemeralTabCoordinatorSupplier.get()
                        : null;
        if (mEphemeralTabCoordinator != null) {
            // Append parameter to open the page with reduced UI elements in the bottomsheet.
            Uri.Builder builder = Uri.parse(url).buildUpon();
            if (mSiteInfo.hasMoreAbout() && url.equals(mSiteInfo.getMoreAbout().getUrl())) {
                builder.appendQueryParameter("ilrm", "minimal,nohead");
            }
            GURL bottomSheetUrl = new GURL(builder.toString());
            GURL fullPageUrl = new GURL(url);

            createEphemeralTabObserver(bottomSheetUrl);
            mEphemeralTabCoordinator.addObserver(mEphemeralTabObserver);

            mEphemeralTabCoordinator.requestOpenSheetWithFullPageUrl(
                    bottomSheetUrl, fullPageUrl, getTitle(), Profile.fromWebContents(mWebContents));

            mMainController.dismiss();
        } else {
            openInNewTab(url);
        }
    }

    private void createEphemeralTabObserver(GURL originUrl) {
        assert mEphemeralTabCoordinator != null;
        mEphemeralTabObserver =
                new EphemeralTabObserver() {
                    @Override
                    public void onToolbarCreated(ViewGroup toolbarView) {
                        TextView origin = toolbarView.findViewById(R.id.origin);
                        origin.setVisibility(View.GONE);

                        ImageView securityIcon = toolbarView.findViewById(R.id.security_icon);
                        securityIcon.setVisibility(View.GONE);

                        TextView title = toolbarView.findViewById(R.id.title);
                        title.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
                        // Style change affects the toolbar height. Requests layout again.
                        ViewUtils.requestLayout(
                                toolbarView, "PageInfoAboutThisSiteController.onToolbarCreated");
                    }

                    @Override
                    public void onNavigationStarted(GURL clickedUrl) {
                        if (!clickedUrl.equals(originUrl)) {
                            mEphemeralTabCoordinator.close();
                            mEphemeralTabCoordinator.removeObserver(this);
                            openInNewTab(clickedUrl.getSpec());
                        }
                    }

                    @Override
                    public void onTitleSet(EphemeralTabSheetContent sheetContent, String title) {
                        sheetContent.updateTitle(getTitle());
                    }
                };
    }

    private void openInNewTab(String url) {
        mTabCreator.createNewTab(
                new LoadUrlParams(url, PageTransition.LINK),
                TabLaunchType.FROM_LINK,
                TabUtils.fromWebContents(mWebContents));
    }

    private void setupRow() {
        if (!mDelegate.isSiteSettingsAvailable() || mDelegate.isIncognito()) {
            return;
        }

        if (mMainController.getSecurityLevel() != ConnectionSecurityLevel.SECURE) {
            return;
        }

        mSiteInfo = getSiteInfo();
        if (mSiteInfo == null) {
            return;
        }

        Resources resources = mRowView.getContext().getResources();
        String subtitle =
                mSiteInfo.hasDescription()
                        ? mSiteInfo.getDescription().getDescription()
                        : resources.getString(
                                R.string.page_info_about_this_page_description_placeholder);
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = getTitle();
        rowParams.subtitle = subtitle;
        rowParams.singleLineSubTitle = true;
        rowParams.visible = true;
        rowParams.iconResId = PageInfoAboutThisSiteControllerJni.get().getJavaDrawableIconId();
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::onAboutThisSiteRowClicked;
        mRowView.setParams(rowParams);
    }

    private String getTitle() {
        return mRowView.getContext()
                .getResources()
                .getString(R.string.page_info_about_this_page_title);
    }

    private @Nullable SiteInfo getSiteInfo() {
        byte[] result =
                PageInfoAboutThisSiteControllerJni.get()
                        .getSiteInfo(
                                mMainController.getBrowserContext(),
                                mMainController.getURL(),
                                mWebContents);
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

    private void onAboutThisSiteRowClicked() {
        openUrl(
                mSiteInfo.getMoreAbout().getUrl(),
                PageInfoAction.PAGE_INFO_ABOUT_THIS_SITE_PAGE_OPENED);
        PageInfoAboutThisSiteControllerJni.get()
                .onAboutThisSiteRowClicked(mSiteInfo.hasDescription());
    }

    @NativeMethods
    interface Natives {
        boolean isFeatureEnabled();

        int getJavaDrawableIconId();

        byte[] getSiteInfo(BrowserContextHandle browserContext, GURL url, WebContents webContents);

        void onAboutThisSiteRowClicked(boolean withDescription);
    }
}
