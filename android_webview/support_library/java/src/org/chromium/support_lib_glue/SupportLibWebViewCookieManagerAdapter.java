package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.support_lib_boundary.WebViewCookieManagerBoundaryInterface;

import java.util.List;

/**
 * Adapter between WebViewCookieManagerBoundaryInterface and AwCookieManager.
 */
class SupportLibWebViewCookieManagerAdapter implements WebViewCookieManagerBoundaryInterface {
    private final AwCookieManager mAwCookieManager;

    public SupportLibWebViewCookieManagerAdapter(AwCookieManager awCookieManager) {
        mAwCookieManager = awCookieManager;
    }

    @Override
    public List<String> getCookieInfo(String url) {
        recordApiCall(SupportLibWebViewChromiumFactory.ApiCall.COOKIE_MANAGER_GET_COOKIE_INFO);
        return mAwCookieManager.getCookieInfo(url);
    }
}
