package com.ark.browser.ui.fragment.pageinfo;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.Date;

public class PageInfoFragment extends BaseSwipeBackFragment {

    private ArkWebContents mWebContents;


    protected String mOfflinePageUrl;
    protected @PageInfoControllerDelegate.OfflinePageState int mOfflinePageState;
    private String mOfflinePageCreationDate;

    @Override
    protected int getLayoutId() {
        return 0;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        PageInfoView.Params viewParams = new PageInfoView.Params();
        viewParams.onUiClosingCallback = () -> {
            // |this| may have already been destroyed by the time this is called.
//            if (mCookiesController != null) mCookiesController.onUiClosing();
        };



        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(mWebContents.getWebContents());
        if (offlinePage != null) {
            mOfflinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(mWebContents.getWebContents())) {
                mOfflinePageState = PageInfoControllerDelegate.OfflinePageState.TRUSTED_OFFLINE_PAGE;
            } else {
                mOfflinePageState = PageInfoControllerDelegate.OfflinePageState.UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                mOfflinePageCreationDate = df.format(creationDate);
            }
        }

        boolean isShowingOfflinePage = mOfflinePageState != PageInfoControllerDelegate.OfflinePageState.NOT_OFFLINE_PAGE;

        if (isShowingOfflinePage && OfflinePageUtils.isConnected()) {
            viewParams.openOnlineButtonClickCallback = () -> {
                mWebContents.reload();
//                runAfterDismiss.accept(() -> {
//                    // Attempt to reload to an online version of the viewed offline web page.
//                    // This attempt might fail if the user is offline, in which case an offline
//                    // copy will be reloaded.
//                    OfflinePageUtils.reload(mWebContents, mOfflinePageLoadUrlDelegate);
//                });
            };
        } else {
            viewParams.openOnlineButtonShown = false;
        }


        GURL url = isShowingOfflinePage
                ? new GURL(mOfflinePageUrl)
                : DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(mWebContents.getUrl());

//        boolean mIsInternalPage = UrlUtilities.isInternalScheme(url);
//        if (!mIsInternalPage && !isShowingOfflinePage
//                && mDelegate.isInstantAppAvailable(url.getSpec())) {
//            final Intent instantAppIntent = mDelegate.getInstantAppIntentForUrl(mFullUrl.getSpec());
//            viewParams.instantAppButtonClickCallback = () -> {
//                try {
//                    getActivity().startActivity(instantAppIntent);
//                    RecordUserAction.record("Android.InstantApps.LaunchedFromWebsiteSettingsPopup");
//                } catch (ActivityNotFoundException e) {
//                    mView.disableInstantAppButton();
//                }
//            };
//            RecordUserAction.record("Android.InstantApps.OpenInstantAppButtonShown");
//        } else {
//            viewParams.instantAppButtonShown = false;
//        }
        viewParams.instantAppButtonShown = false;
        viewParams.httpsImageCompressionMessageShown = false;

        PageInfoView mView = new PageInfoView(context, viewParams);


    }

//    public boolean isInstantAppAvailable(String url) {
//        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
//        return instantAppsHandler.isInstantAppAvailable(
//                url, false /* checkHoldback */, false /* includeUserPrefersBrowser */);
//    }

}
