package com.ark.browser.tab;

import android.text.TextUtils;

import com.ark.browser.core.UserAgentManager;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

public class ArkInterceptNavigationDelegateImpl extends InterceptNavigationDelegateImpl {

    private static final String TAG = "ArkInterceptNavigationDelegateImpl";

    private final Tab mTab;

    private boolean mLoadNewPage = false;

    private long start;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     *
     * @param client
     */
    public ArkInterceptNavigationDelegateImpl(Tab tab, InterceptNavigationDelegateClient client) {
        super(client);
        this.mTab = tab;
        this.mTab.addObserver(new EmptyTabObserver() {

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                mLoadNewPage = false;
                super.onPageLoadStarted(tab, url);
                ArkLogger.d(TAG, "onPageLoadStarted url=" + tab.getUrl().getSpec());
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                super.onPageLoadFinished(tab, url);
                ArkLogger.d(TAG, "onPageLoadFinished url=" + tab.getUrl().getSpec());
            }

            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
                super.onDidStartNavigation(tab, navigationHandle);
                ArkLogger.d(TAG, "onDidStartNavigation url=" + tab.getUrl().getSpec());
            }

            @Override
            public void onDidRedirectNavigation(Tab tab, NavigationHandle navigationHandle) {
                super.onDidRedirectNavigation(tab, navigationHandle);
                ArkLogger.d(TAG, "onDidRedirectNavigation url=" + tab.getUrl()
                        + " originalUrl=" + tab.getOriginalUrl());

                String host = navigationHandle.getUrl().getHost();
                if (TextUtils.equals(host, tab.getOriginalUrl().getHost())) {
                    return;
                }

                int index = UserAgentManager.getUserAgentIndexByUrl(tab.getOriginalUrl());
                UserAgentManager.setUserAgentByUrl(host, index);
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                ArkLogger.d(TAG, "onDidFinishNavigation navigationHandle=" + navigationHandle);
                if (mLoadNewPage) {
                    ArkLogger.d(TAG, "onDidFinishNavigation mLoadNewPage deltaTime=" + (System.currentTimeMillis() - start));
                    mLoadNewPage = false;

                    WebContents webContents = mClient.getWebContents();
                    if (webContents != null) {
                        NavigationController navigationController = webContents.getNavigationController();
                        navigationController.pruneForwardEntries();
                    }

                    GURL url = navigationHandle.getUrl();
                    LoadUrlParams params = new LoadUrlParams(url);
                    params.setHasUserGesture(navigationHandle.hasUserGesture());
                    params.setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
                    params.setBaseUrlForDataUrl(navigationHandle.getBaseUrlForDataUrl().getSpec());
                    params.setReferrer(new Referrer(navigationHandle.getReferrerUrl().getSpec(),
                            ReferrerPolicy.DEFAULT));
                    params.setIsRendererInitiated(navigationHandle.isRendererInitiated());

                    Log.e(TAG, "shouldIgnoreNavigation params=" + params);


                    ((ArkTabImpl) mTab).openNewPage(params);
                }
            }
        });
    }



//    @Override
//    public void onNavigationFinished(NavigationHandle navigation) {
//        super.onNavigationFinished(navigation);
//        ArkLogger.e(TAG, "onNavigationFinished navigation=" + navigation);
//    }

    //    @Override
//    protected ExternalNavigationHandler.OverrideUrlLoadingResult onResult(NavigationHandle navigationHandle, ExternalNavigationHandler.OverrideUrlLoadingResult result) {
//
//        if (result.getResultType() == ExternalNavigationHandler.OverrideUrlLoadingResultType.NO_OVERRIDE) {
//            int pageTransition = navigationHandle.pageTransition();
//            Log.e(TAG, "shouldIgnoreNavigation url=" + mTab.getUrl()
//                    + "\noriginUrl=" + mTab.getOriginalUrl()
//                    + "\npageTransition=" + pageTransition
//                    + "\ngetLastCommittedEntryIndex=" + getLastCommittedEntryIndex()
//                    + "\nisInitialNavigation=" + isInitialNavigation());
//            if (isInitialNavigation()) {
//                return super.onResult(navigationHandle, result);
//            }
//            if (pageTransition == PageTransition.RELOAD
//                    || pageTransition == PageTransition.BLOCKED
//                    || pageTransition == PageTransition.CLIENT_REDIRECT
//                    || pageTransition == PageTransition.SERVER_REDIRECT
//                    || pageTransition == PageTransition.CHAIN_START
//                    || pageTransition == PageTransition.CHAIN_END) {
//                return super.onResult(navigationHandle, result);
//            }
//
//            if (!navigationHandle.isRedirect() && !navigationHandle.isDownload()
//                    && !navigationHandle.isFragmentNavigation()
//                    && !navigationHandle.isExternalProtocol()) {
//
//                GURL url = navigationHandle.getUrl();
//                LoadUrlParams params = new LoadUrlParams(url);
//                params.setHasUserGesture(navigationHandle.hasUserGesture());
//                params.setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
//                params.setBaseUrlForDataUrl(navigationHandle.getBaseUrlForDataUrl().getSpec());
//                params.setReferrer(new Referrer(navigationHandle.getReferrerUrl().getSpec(),
//                        ReferrerPolicy.ALWAYS));
//                params.setIsRendererInitiated(navigationHandle.isRendererInitiated());
//
//                Log.e(TAG, "shouldIgnoreNavigation params=" + params);
//
//                TabListManager.getInstance().openNewPage(mTab, params);
//
//                return super.onResult(navigationHandle, ExternalNavigationHandler.OverrideUrlLoadingResult.forNewPage());
//            }
//
//        }
//        return super.onResult(navigationHandle, result);
//    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationHandle navigationHandle, GURL escapedUrl) {
        Log.e(TAG, "shouldIgnoreNavigation navigationHandle=" + navigationHandle + "\nescapedUrl=" + escapedUrl);
        boolean shouldIgnore = super.shouldIgnoreNavigation(navigationHandle, escapedUrl);
        if (shouldIgnore) {
            return true;
        }

        int pageTransition = navigationHandle.pageTransition();
        Log.e(TAG, "shouldIgnoreNavigation url=" + mTab.getUrl()
                + "\noriginUrl=" + mTab.getOriginalUrl()
                + "\npageTransition=" + pageTransition
                + "\ngetLastCommittedEntryIndex=" + getLastCommittedEntryIndex()
                + "\nisInitialNavigation=" + isInitialNavigation());
        if (isInitialNavigation()) {
            return false;
        }
        if (pageTransition == PageTransition.RELOAD
                || pageTransition == PageTransition.BLOCKED
                || pageTransition == PageTransition.CLIENT_REDIRECT
                || pageTransition == PageTransition.SERVER_REDIRECT
                || pageTransition == PageTransition.CHAIN_START
                || pageTransition == PageTransition.CHAIN_END) {
            return false;
        }

        if (!navigationHandle.isRedirect() && !navigationHandle.isDownload()
                && !navigationHandle.isFragmentNavigation()) {

//            GURL url = navigationHandle.getUrl();
//            LoadUrlParams params = new LoadUrlParams(url);
//            params.setHasUserGesture(navigationHandle.hasUserGesture());
//            params.setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
//            params.setBaseUrlForDataUrl(navigationHandle.getBaseUrlForDataUrl().getSpec());
//            params.setReferrer(new Referrer(navigationHandle.getReferrerUrl().getSpec(),
//                    ReferrerPolicy.ALWAYS));
//            params.setIsRendererInitiated(navigationHandle.isRendererInitiated());
//
//            Log.e(TAG, "shouldIgnoreNavigation params=" + params);
//
////            ThreadPool.postDelayed(new Runnable() {
////                @Override
////                public void run() {
//////                    TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
////
////                    TabListManager.getInstance().openNewPage(mTab, params);
////                }
////            }, 1000);
//
//            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
//                @Override
//                public void run() {
//                    mTab.stopLoading();
//                    TabListManager.getInstance().openNewPage(mTab, params);
//                }
//            });

            start = System.currentTimeMillis();
            mLoadNewPage = true;
            return true;
        }

        return false;
    }
}
