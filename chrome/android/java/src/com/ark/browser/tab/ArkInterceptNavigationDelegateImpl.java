package com.ark.browser.tab;

import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

public class ArkInterceptNavigationDelegateImpl extends InterceptNavigationDelegateImpl {

    private static final String TAG = "ArkInterceptNavigationDelegateImpl";

    private final Tab mTab;

//    private boolean mLoadNewPage = false;

    private long start;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     *
     * @param client
     */
    public ArkInterceptNavigationDelegateImpl(Tab tab, InterceptNavigationDelegateClient client) {
        super(client);
        this.mTab = tab;
    }

    @Override
    public void onNavigationStart(NavigationHandle navigation) {
//        mLoadNewPage = false;
    }

    @Override
    public void onNavigationFinished(NavigationHandle navigationHandle) {
        ArkLogger.d(TAG, "onDidFinishNavigation navigationHandle=" + navigationHandle);
//        if (mLoadNewPage) {
//            ArkLogger.d(TAG, "onDidFinishNavigation mLoadNewPage deltaTime=" + (System.currentTimeMillis() - start));
//            mLoadNewPage = false;
//
//            WebContents webContents = mClient.getWebContents();
//            if (webContents != null) {
//                NavigationController navigationController = webContents.getNavigationController();
//                navigationController.pruneForwardEntries();
//            }
//
////            GURL url = navigationHandle.getUrl();
////            LoadUrlParams params = new LoadUrlParams(url);
////            params.setHasUserGesture(navigationHandle.hasUserGesture());
////            params.setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
////            params.setBaseUrlForDataUrl(navigationHandle.getBaseUrlForDataUrl().getSpec());
////            params.setReferrer(new Referrer(navigationHandle.getReferrerUrl().getSpec(),
////                    ReferrerPolicy.DEFAULT));
////            params.setIsRendererInitiated(navigationHandle.isRendererInitiated());
////
////            Log.e(TAG, "shouldIgnoreNavigation params=" + params);
////
////
////            ((ArkTabImpl) mTab).openNewPage(params);
//            return;
//        }


        super.onNavigationFinished(navigationHandle);
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
        ArkLogger.e(TAG, "shouldIgnoreNavigation navigationHandle=" + navigationHandle + "\nescapedUrl=" + escapedUrl);
        boolean shouldIgnore = super.shouldIgnoreNavigation(navigationHandle, escapedUrl);
        if (shouldIgnore) {
            return true;
        }

        int pageTransition = navigationHandle.pageTransition();
        ArkLogger.e(TAG, "shouldIgnoreNavigation url=" + mTab.getUrl()
                + "\noriginUrl=" + mTab.getOriginalUrl()
                + "\npageTransition=" + pageTransition
                + "\ngetLastCommittedEntryIndex=" + getLastCommittedEntryIndex()
                + "\nisInitialNavigation=" + isInitialNavigation()
                + "\nisSameDocument=" + navigationHandle.isSameDocument());
        if (isInitialNavigation()) {
            return false;
        }
        if (pageTransition == PageTransition.RELOAD
                || pageTransition == PageTransition.AUTO_SUBFRAME
                || pageTransition == PageTransition.MANUAL_SUBFRAME
                || pageTransition == PageTransition.FORM_SUBMIT
                || pageTransition == PageTransition.BLOCKED
                || pageTransition == PageTransition.CLIENT_REDIRECT
                || pageTransition == PageTransition.SERVER_REDIRECT
                || pageTransition == PageTransition.CHAIN_START
                || pageTransition == PageTransition.CHAIN_END) {
            return false;
        }

        ArkLogger.e(TAG, "shouldIgnoreNavigation isRedirect=" + navigationHandle.isRedirect()
                + " isDownload=" + navigationHandle.isDownload()
                + " isFragmentNavigation=" + navigationHandle.isFragmentNavigation());
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
//            mLoadNewPage = true;


//            WebContents webContents = mClient.getWebContents();
//            if (webContents != null) {
//                NavigationController navigationController = webContents.getNavigationController();
//                navigationController.pruneForwardEntries();
//            }

            GURL url = navigationHandle.getUrl();
            LoadUrlParams params = new LoadUrlParams(url);
            params.setHasUserGesture(navigationHandle.hasUserGesture());
            params.setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
            params.setBaseUrlForDataUrl(navigationHandle.getBaseUrlForDataUrl().getSpec());
            params.setReferrer(new Referrer(navigationHandle.getReferrerUrl().getSpec(),
                    ReferrerPolicy.DEFAULT));
            params.setIsRendererInitiated(navigationHandle.isRendererInitiated());

            ArkLogger.e(TAG, "shouldIgnoreNavigation params=" + params);

            ((ArkTabImpl) mTab).loadInNewPage(params);

//            WebContents webContents = WarmupManager.getInstance().takeSpareWebContents(
//                    mTab.isIncognito(), mTab.isHidden(), mTab.isCustomTab());
//            if (webContents == null) {
//                Profile profile =
//                        IncognitoUtils.getProfileFromWindowAndroid(mTab.getWindowAndroid(), mTab.isIncognito());
//                webContents = WebContentsFactory.createWebContents(profile, mTab.isHidden());
//            }
//
//            GURL fixedUrl = UrlFormatter.fixupUrl(params.getUrl());
//            params.setUrl(fixedUrl.getSpec());
//            ContentUtils.setUserAgentOverride(webContents, UserAgentManager.getUserAgentByUrl(fixedUrl));
//
//            webContents.getNavigationController().loadUrl(params);

//            webContents.addObserver(new WebContentsObserver() {
//
//                private static final String TAG = "NewPage_WebContentsObserver";
//
//                private boolean mDidStartLoading = true;
//                private boolean mDidFinishLoad = false;
//
//                @Override
//                public void didStartLoading(GURL url) {
//                    ArkLogger.e(TAG, "didStartLoading url=" + url.getSpec());
//                    mDidStartLoading = true;
//                }
//
//                @Override
//                public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url,
//                                          boolean isKnownValid, boolean isInPrimaryMainFrame,
//                                          int rfhLifecycleState) {
//                    ArkLogger.e(TAG, "didFinishLoad url=" + url.getSpec());
//                    mDidFinishLoad = true;
//                }
//
//                @Override
//                public void didStopLoading(GURL url, boolean isKnownValid) {
//                    ArkLogger.e(TAG, "didStopLoading url=" + url.getSpec());
//                }
//
//                @Override
//                public void didFailLoad(boolean isInPrimaryMainFrame, int errorCode,
//                                        GURL failingUrl, int rfhLifecycleState) {
//                    ArkLogger.e(TAG, "didFailLoad errorCode=" + errorCode
//                            + " failingUrl=" + failingUrl.getSpec());
//                }
//
//                @Override
//                public void primaryMainDocumentElementAvailable() {
//                    ArkLogger.e(TAG, "primaryMainDocumentElementAvailable");
//                }
//
//                @Override
//                public void didFirstVisuallyNonEmptyPaint() {
//                    ArkLogger.e(TAG, "didFirstVisuallyNonEmptyPaint");
//                    mTab.swapWebContents(mWebContents.get(), mDidStartLoading, mDidFinishLoad);
//                }
//            });

//            ITab iTab = ((ArkTabImpl) mTab).getTab();
//            PageInfo pageInfo = PageInfo.from(TabIdManager.getInstance().generateValidId(),
//                    Tab.INVALID_PAGE_ID, iTab.getId(),
//                    iTab.getTabInfo().getIndex() + 1, iTab.getTabInfo().isIncognito());
//            ArkWebContents arkWeb = new ArkWebContents(pageInfo, webContents);
//            ((ArkTabImpl) mTab).swapWebContents(arkWeb, true, false);
            return true;
        }

        return false;
    }
}
