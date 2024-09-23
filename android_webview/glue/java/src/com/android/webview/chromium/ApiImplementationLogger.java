// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.reflect.Method;
import java.util.function.Consumer;
import java.util.function.IntConsumer;
import java.util.function.Predicate;

/**
 * Helper class to log which WebView APIs have been implemented by app-provided callback objects.
 */
public final class ApiImplementationLogger {

    private ApiImplementationLogger() {}

    private static final String TAG = "ApiImplLogger";

    /**
     * Enumeration of all overridable methods in {@link android.webkit.WebViewClient}.
     *
     * <p>These values are logged to UMA, and should never be changed or reused.
     *
     * @noinspection SpellCheckingInspection
     */
    @VisibleForTesting
    @IntDef({
        WebViewClientMethod.UNKNOWN,
        WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_STRING,
        WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_WEBRESOURCEREQUEST,
        WebViewClientMethod.ONPAGESTARTED_WEBVIEW_STRING_BITMAP,
        WebViewClientMethod.ONPAGEFINISHED_WEBVIEW_STRING,
        WebViewClientMethod.ONLOADRESOURCE_WEBVIEW_STRING,
        WebViewClientMethod.ONPAGECOMMITVISIBLE_WEBVIEW_STRING,
        WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_STRING,
        WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_WEBRESOURCEREQUEST,
        WebViewClientMethod.ONTOOMANYREDIRECTS_WEBVIEW_MESSAGE_MESSAGE,
        WebViewClientMethod.ONRECEIVEDERROR_WEBVIEW_INT_STRING_STRING,
        WebViewClientMethod.ONRECEIVEDERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCEERROR,
        WebViewClientMethod.ONRECEIVEDHTTPERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCERESPONSE,
        WebViewClientMethod.ONFORMRESUBMISSION_WEBVIEW_MESSAGE_MESSAGE,
        WebViewClientMethod.DOUPDATEVISITEDHISTORY_WEBVIEW_STRING_BOOLEAN,
        WebViewClientMethod.ONRECEIVEDSSLERROR_WEBVIEW_SSLERRORHANDLER_SSLERROR,
        WebViewClientMethod.ONRECEIVEDCLIENTCERTREQUEST_WEBVIEW_CLIENTCERTREQUEST,
        WebViewClientMethod.ONRECEIVEDHTTPAUTHREQUEST_WEBVIEW_HTTPAUTHHANDLER_STRING_STRING,
        WebViewClientMethod.SHOULDOVERRIDEKEYEVENT_WEBVIEW_KEYEVENT,
        WebViewClientMethod.ONUNHANDLEDKEYEVENT_WEBVIEW_KEYEVENT,
        WebViewClientMethod.ONUNHANDLEDINPUTEVENT_WEBVIEW_INPUTEVENT,
        WebViewClientMethod.ONSCALECHANGED_WEBVIEW_FLOAT_FLOAT,
        WebViewClientMethod.ONRECEIVEDLOGINREQUEST_WEBVIEW_STRING_STRING_STRING,
        WebViewClientMethod.ONRENDERPROCESSGONE_WEBVIEW_RENDERPROCESSGONEDETAIL,
        WebViewClientMethod.ONSAFEBROWSINGHIT_WEBVIEW_WEBRESOURCEREQUEST_INT_SAFEBROWSINGRESPONSE,
        WebViewClientMethod.COUNT
    })
    public @interface WebViewClientMethod {

        int UNKNOWN = 0;
        int SHOULDOVERRIDEURLLOADING_WEBVIEW_STRING = 1;
        int SHOULDOVERRIDEURLLOADING_WEBVIEW_WEBRESOURCEREQUEST = 2;
        int ONPAGESTARTED_WEBVIEW_STRING_BITMAP = 3;
        int ONPAGEFINISHED_WEBVIEW_STRING = 4;
        int ONLOADRESOURCE_WEBVIEW_STRING = 5;
        int ONPAGECOMMITVISIBLE_WEBVIEW_STRING = 6;
        int SHOULDINTERCEPTREQUEST_WEBVIEW_STRING = 7;
        int SHOULDINTERCEPTREQUEST_WEBVIEW_WEBRESOURCEREQUEST = 8;
        int ONTOOMANYREDIRECTS_WEBVIEW_MESSAGE_MESSAGE = 9;
        int ONRECEIVEDERROR_WEBVIEW_INT_STRING_STRING = 10;
        int ONRECEIVEDERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCEERROR = 11;
        int ONRECEIVEDHTTPERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCERESPONSE = 12;
        int ONFORMRESUBMISSION_WEBVIEW_MESSAGE_MESSAGE = 13;
        int DOUPDATEVISITEDHISTORY_WEBVIEW_STRING_BOOLEAN = 14;
        int ONRECEIVEDSSLERROR_WEBVIEW_SSLERRORHANDLER_SSLERROR = 15;
        int ONRECEIVEDCLIENTCERTREQUEST_WEBVIEW_CLIENTCERTREQUEST = 16;
        int ONRECEIVEDHTTPAUTHREQUEST_WEBVIEW_HTTPAUTHHANDLER_STRING_STRING = 17;
        int SHOULDOVERRIDEKEYEVENT_WEBVIEW_KEYEVENT = 18;
        int ONUNHANDLEDKEYEVENT_WEBVIEW_KEYEVENT = 19;

        /**
         * @deprecated Removed in public API.
         */
        @Deprecated int ONUNHANDLEDINPUTEVENT_WEBVIEW_INPUTEVENT = 20;

        int ONSCALECHANGED_WEBVIEW_FLOAT_FLOAT = 21;
        int ONRECEIVEDLOGINREQUEST_WEBVIEW_STRING_STRING_STRING = 22;
        int ONRENDERPROCESSGONE_WEBVIEW_RENDERPROCESSGONEDETAIL = 23;
        int ONSAFEBROWSINGHIT_WEBVIEW_WEBRESOURCEREQUEST_INT_SAFEBROWSINGRESPONSE = 24;
        // Add new values above this comment and increment the COUNT.
        int COUNT = 25;
    }

    @VisibleForTesting
    public static @WebViewClientMethod int toWebViewClientMethodEnum(@NonNull Method method) {
        //noinspection EnhancedSwitchMigration
        switch (method.toString()) {
            case "public boolean android.webkit.WebViewClient.shouldOverrideUrlLoading("
                    + "android.webkit.WebView,java.lang.String)":
                return WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_STRING;
            case "public boolean android.webkit.WebViewClient.shouldOverrideUrlLoading("
                    + "android.webkit.WebView,android.webkit.WebResourceRequest)":
                return WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_WEBRESOURCEREQUEST;
            case "public void android.webkit.WebViewClient.onPageStarted("
                    + "android.webkit.WebView,java.lang.String,android.graphics.Bitmap)":
                return WebViewClientMethod.ONPAGESTARTED_WEBVIEW_STRING_BITMAP;
            case "public void android.webkit.WebViewClient.onPageFinished("
                    + "android.webkit.WebView,java.lang.String)":
                return WebViewClientMethod.ONPAGEFINISHED_WEBVIEW_STRING;
            case "public void android.webkit.WebViewClient.onLoadResource("
                    + "android.webkit.WebView,java.lang.String)":
                return WebViewClientMethod.ONLOADRESOURCE_WEBVIEW_STRING;
            case "public void android.webkit.WebViewClient.onPageCommitVisible("
                    + "android.webkit.WebView,java.lang.String)":
                return WebViewClientMethod.ONPAGECOMMITVISIBLE_WEBVIEW_STRING;
            case "public android.webkit.WebResourceResponse"
                    + " android.webkit.WebViewClient.shouldInterceptRequest("
                    + "android.webkit.WebView,java.lang.String)":
                return WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_STRING;
            case "public android.webkit.WebResourceResponse"
                    + " android.webkit.WebViewClient.shouldInterceptRequest("
                    + "android.webkit.WebView,android.webkit.WebResourceRequest)":
                return WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_WEBRESOURCEREQUEST;
            case "public void android.webkit.WebViewClient.onTooManyRedirects("
                    + "android.webkit.WebView,android.os.Message,android.os.Message)":
                return WebViewClientMethod.ONTOOMANYREDIRECTS_WEBVIEW_MESSAGE_MESSAGE;
            case "public void android.webkit.WebViewClient.onReceivedError("
                    + "android.webkit.WebView,int,java.lang.String,java.lang.String)":
                return WebViewClientMethod.ONRECEIVEDERROR_WEBVIEW_INT_STRING_STRING;
            case "public void android.webkit.WebViewClient.onReceivedError("
                    + "android.webkit.WebView,android.webkit.WebResourceRequest,"
                    + "android.webkit.WebResourceError)":
                return WebViewClientMethod
                        .ONRECEIVEDERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCEERROR;
            case "public void android.webkit.WebViewClient.onReceivedHttpError("
                    + "android.webkit.WebView,android.webkit.WebResourceRequest,"
                    + "android.webkit.WebResourceResponse)":
                return WebViewClientMethod
                        .ONRECEIVEDHTTPERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCERESPONSE;
            case "public void android.webkit.WebViewClient.onFormResubmission("
                    + "android.webkit.WebView,android.os.Message,android.os.Message)":
                return WebViewClientMethod.ONFORMRESUBMISSION_WEBVIEW_MESSAGE_MESSAGE;
            case "public void android.webkit.WebViewClient.doUpdateVisitedHistory("
                    + "android.webkit.WebView,java.lang.String,boolean)":
                return WebViewClientMethod.DOUPDATEVISITEDHISTORY_WEBVIEW_STRING_BOOLEAN;
            case "public void android.webkit.WebViewClient.onReceivedSslError("
                    + "android.webkit.WebView,android.webkit.SslErrorHandler,"
                    + "android.net.http.SslError)":
                return WebViewClientMethod.ONRECEIVEDSSLERROR_WEBVIEW_SSLERRORHANDLER_SSLERROR;
            case "public void android.webkit.WebViewClient.onReceivedClientCertRequest("
                    + "android.webkit.WebView,android.webkit.ClientCertRequest)":
                return WebViewClientMethod.ONRECEIVEDCLIENTCERTREQUEST_WEBVIEW_CLIENTCERTREQUEST;
            case "public void android.webkit.WebViewClient.onReceivedHttpAuthRequest("
                    + "android.webkit.WebView,android.webkit.HttpAuthHandler,"
                    + "java.lang.String,java.lang.String)":
                return WebViewClientMethod
                        .ONRECEIVEDHTTPAUTHREQUEST_WEBVIEW_HTTPAUTHHANDLER_STRING_STRING;
            case "public boolean android.webkit.WebViewClient.shouldOverrideKeyEvent("
                    + "android.webkit.WebView,android.view.KeyEvent)":
                return WebViewClientMethod.SHOULDOVERRIDEKEYEVENT_WEBVIEW_KEYEVENT;
            case "public void android.webkit.WebViewClient.onUnhandledKeyEvent("
                    + "android.webkit.WebView,android.view.KeyEvent)":
                return WebViewClientMethod.ONUNHANDLEDKEYEVENT_WEBVIEW_KEYEVENT;
            case "public void android.webkit.WebViewClient.onUnhandledInputEvent("
                    + "android.webkit.WebView,android.view.InputEvent)":
                return WebViewClientMethod.ONUNHANDLEDINPUTEVENT_WEBVIEW_INPUTEVENT;
            case "public void android.webkit.WebViewClient.onScaleChanged("
                    + "android.webkit.WebView,float,float)":
                return WebViewClientMethod.ONSCALECHANGED_WEBVIEW_FLOAT_FLOAT;
            case "public void android.webkit.WebViewClient.onReceivedLoginRequest("
                    + "android.webkit.WebView,java.lang.String,java.lang.String,java.lang.String)":
                return WebViewClientMethod.ONRECEIVEDLOGINREQUEST_WEBVIEW_STRING_STRING_STRING;
            case "public boolean android.webkit.WebViewClient.onRenderProcessGone("
                    + "android.webkit.WebView,android.webkit.RenderProcessGoneDetail)":
                return WebViewClientMethod.ONRENDERPROCESSGONE_WEBVIEW_RENDERPROCESSGONEDETAIL;
            case "public void android.webkit.WebViewClient.onSafeBrowsingHit("
                    + "android.webkit.WebView,android.webkit.WebResourceRequest,int,"
                    + "android.webkit.SafeBrowsingResponse)":
                return WebViewClientMethod
                        .ONSAFEBROWSINGHIT_WEBVIEW_WEBRESOURCEREQUEST_INT_SAFEBROWSINGRESPONSE;
            default:
                return WebViewClientMethod.UNKNOWN;
        }
    }

    /**
     * Allowlist of WebViewClient methods that should be logged
     *
     * @param method Method from WebViewClient
     * @return true if the method should be logged
     */
    private static boolean shouldLogWebViewClientMethod(Method method) {
        @WebViewClientMethod int methodEnum = toWebViewClientMethodEnum(method);
        return switch (methodEnum) {
            case
                    // Include COUNT and UNKNOWN to have all values present
                    WebViewClientMethod.COUNT,
                    WebViewClientMethod.UNKNOWN,
                    // The following values are marked as @removed in the API and should
                    // not be accessed
                    WebViewClientMethod.ONUNHANDLEDINPUTEVENT_WEBVIEW_INPUTEVENT -> false;

            case WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_STRING,
                    WebViewClientMethod.SHOULDOVERRIDEURLLOADING_WEBVIEW_WEBRESOURCEREQUEST,
                    WebViewClientMethod.ONPAGESTARTED_WEBVIEW_STRING_BITMAP,
                    WebViewClientMethod.ONPAGEFINISHED_WEBVIEW_STRING,
                    WebViewClientMethod.ONLOADRESOURCE_WEBVIEW_STRING,
                    WebViewClientMethod.ONPAGECOMMITVISIBLE_WEBVIEW_STRING,
                    WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_STRING,
                    WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_WEBRESOURCEREQUEST,
                    WebViewClientMethod.ONTOOMANYREDIRECTS_WEBVIEW_MESSAGE_MESSAGE,
                    WebViewClientMethod.ONRECEIVEDERROR_WEBVIEW_INT_STRING_STRING,
                    WebViewClientMethod.ONRECEIVEDERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCEERROR,
                    WebViewClientMethod
                            .ONRECEIVEDHTTPERROR_WEBVIEW_WEBRESOURCEREQUEST_WEBRESOURCERESPONSE,
                    WebViewClientMethod.ONFORMRESUBMISSION_WEBVIEW_MESSAGE_MESSAGE,
                    WebViewClientMethod.DOUPDATEVISITEDHISTORY_WEBVIEW_STRING_BOOLEAN,
                    WebViewClientMethod.ONRECEIVEDSSLERROR_WEBVIEW_SSLERRORHANDLER_SSLERROR,
                    WebViewClientMethod.ONRECEIVEDCLIENTCERTREQUEST_WEBVIEW_CLIENTCERTREQUEST,
                    WebViewClientMethod
                            .ONRECEIVEDHTTPAUTHREQUEST_WEBVIEW_HTTPAUTHHANDLER_STRING_STRING,
                    WebViewClientMethod.SHOULDOVERRIDEKEYEVENT_WEBVIEW_KEYEVENT,
                    WebViewClientMethod.ONUNHANDLEDKEYEVENT_WEBVIEW_KEYEVENT,
                    WebViewClientMethod.ONSCALECHANGED_WEBVIEW_FLOAT_FLOAT,
                    WebViewClientMethod.ONRECEIVEDLOGINREQUEST_WEBVIEW_STRING_STRING_STRING,
                    WebViewClientMethod.ONRENDERPROCESSGONE_WEBVIEW_RENDERPROCESSGONEDETAIL,
                    WebViewClientMethod
                            .ONSAFEBROWSINGHIT_WEBVIEW_WEBRESOURCEREQUEST_INT_SAFEBROWSINGRESPONSE -> true;
            default -> false; // Just return false if we get an unknown value.
        };
    }

    /**
     * Enumeration of all overridable methods in {@link android.webkit.WebChromeClient}.
     *
     * <p>These values are logged to UMA, and should never be changed or reused.
     *
     * @noinspection SpellCheckingInspection
     */
    @VisibleForTesting
    @IntDef({
        WebChromeClientMethod.UNKNOWN,
        WebChromeClientMethod.ONPROGRESSCHANGED_WEBVIEW_INT,
        WebChromeClientMethod.ONRECEIVEDTITLE_WEBVIEW_STRING,
        WebChromeClientMethod.ONRECEIVEDICON_WEBVIEW_BITMAP,
        WebChromeClientMethod.ONRECEIVEDTOUCHICONURL_WEBVIEW_STRING_BOOLEAN,
        WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_CUSTOMVIEWCALLBACK,
        WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_INT_CUSTOMVIEWCALLBACK,
        WebChromeClientMethod.ONHIDECUSTOMVIEW_,
        WebChromeClientMethod.ONCREATEWINDOW_WEBVIEW_BOOLEAN_BOOLEAN_MESSAGE,
        WebChromeClientMethod.ONREQUESTFOCUS_WEBVIEW,
        WebChromeClientMethod.ONCLOSEWINDOW_WEBVIEW,
        WebChromeClientMethod.ONJSALERT_WEBVIEW_STRING_STRING_JSRESULT,
        WebChromeClientMethod.ONJSCONFIRM_WEBVIEW_STRING_STRING_JSRESULT,
        WebChromeClientMethod.ONJSPROMPT_WEBVIEW_STRING_STRING_STRING_JSPROMPTRESULT,
        WebChromeClientMethod.ONJSBEFOREUNLOAD_WEBVIEW_STRING_STRING_JSRESULT,
        WebChromeClientMethod.ONEXCEEDEDDATABASEQUOTA_STRING_STRING_LONG_LONG_LONG_QUOTAUPDATER,
        WebChromeClientMethod.ONREACHEDMAXAPPCACHESIZE_LONG_LONG_QUOTAUPDATER,
        WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSSHOWPROMPT_STRING_CALLBACK,
        WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSHIDEPROMPT_,
        WebChromeClientMethod.ONPERMISSIONREQUEST_PERMISSIONREQUEST,
        WebChromeClientMethod.ONPERMISSIONREQUESTCANCELED_PERMISSIONREQUEST,
        WebChromeClientMethod.ONJSTIMEOUT_,
        WebChromeClientMethod.ONCONSOLEMESSAGE_STRING_INT_STRING,
        WebChromeClientMethod.ONCONSOLEMESSAGE_CONSOLEMESSAGE,
        WebChromeClientMethod.GETDEFAULTVIDEOPOSTER_,
        WebChromeClientMethod.GETVIDEOLOADINGPROGRESSVIEW_,
        WebChromeClientMethod.GETVISITEDHISTORY_VALUECALLBACK,
        WebChromeClientMethod.ONSHOWFILECHOOSER_WEBVIEW_VALUECALLBACK_FILECHOOSERPARAMS,
        WebChromeClientMethod.OPENFILECHOOSER_VALUECALLBACK_STRING_STRING,
        WebChromeClientMethod.SETUPAUTOFILL_MESSAGE,
        WebChromeClientMethod.COUNT,
    })
    public @interface WebChromeClientMethod {

        int UNKNOWN = 0;
        int ONPROGRESSCHANGED_WEBVIEW_INT = 1;
        int ONRECEIVEDTITLE_WEBVIEW_STRING = 2;
        int ONRECEIVEDICON_WEBVIEW_BITMAP = 3;
        int ONRECEIVEDTOUCHICONURL_WEBVIEW_STRING_BOOLEAN = 4;
        int ONSHOWCUSTOMVIEW_VIEW_CUSTOMVIEWCALLBACK = 5;
        int ONSHOWCUSTOMVIEW_VIEW_INT_CUSTOMVIEWCALLBACK = 6;
        int ONHIDECUSTOMVIEW_ = 7;
        int ONCREATEWINDOW_WEBVIEW_BOOLEAN_BOOLEAN_MESSAGE = 8;
        int ONREQUESTFOCUS_WEBVIEW = 9;
        int ONCLOSEWINDOW_WEBVIEW = 10;
        int ONJSALERT_WEBVIEW_STRING_STRING_JSRESULT = 11;
        int ONJSCONFIRM_WEBVIEW_STRING_STRING_JSRESULT = 12;
        int ONJSPROMPT_WEBVIEW_STRING_STRING_STRING_JSPROMPTRESULT = 13;
        int ONJSBEFOREUNLOAD_WEBVIEW_STRING_STRING_JSRESULT = 14;
        int ONEXCEEDEDDATABASEQUOTA_STRING_STRING_LONG_LONG_LONG_QUOTAUPDATER = 15;

        /**
         * @deprecated Removed in public API.
         */
        @Deprecated int ONREACHEDMAXAPPCACHESIZE_LONG_LONG_QUOTAUPDATER = 16;

        int ONGEOLOCATIONPERMISSIONSSHOWPROMPT_STRING_CALLBACK = 17;
        int ONGEOLOCATIONPERMISSIONSHIDEPROMPT_ = 18;
        int ONPERMISSIONREQUEST_PERMISSIONREQUEST = 19;
        int ONPERMISSIONREQUESTCANCELED_PERMISSIONREQUEST = 20;
        int ONJSTIMEOUT_ = 21;
        int ONCONSOLEMESSAGE_STRING_INT_STRING = 22;
        int ONCONSOLEMESSAGE_CONSOLEMESSAGE = 23;
        int GETDEFAULTVIDEOPOSTER_ = 24;
        int GETVIDEOLOADINGPROGRESSVIEW_ = 25;
        int GETVISITEDHISTORY_VALUECALLBACK = 26;
        int ONSHOWFILECHOOSER_WEBVIEW_VALUECALLBACK_FILECHOOSERPARAMS = 27;
        int OPENFILECHOOSER_VALUECALLBACK_STRING_STRING = 28;
        int SETUPAUTOFILL_MESSAGE = 29;
        // Add new values above this comment and increment the COUNT.
        int COUNT = 30;
    }

    @VisibleForTesting
    public static @WebChromeClientMethod int toWebChromeClientMethodEnum(@NonNull Method method) {
        //noinspection EnhancedSwitchMigration
        switch (method.toString()) {
            case "public void android.webkit.WebChromeClient.onProgressChanged("
                    + "android.webkit.WebView,int)":
                return WebChromeClientMethod.ONPROGRESSCHANGED_WEBVIEW_INT;
            case "public void android.webkit.WebChromeClient.onReceivedTitle("
                    + "android.webkit.WebView,java.lang.String)":
                return WebChromeClientMethod.ONRECEIVEDTITLE_WEBVIEW_STRING;
            case "public void android.webkit.WebChromeClient.onReceivedIcon("
                    + "android.webkit.WebView,android.graphics.Bitmap)":
                return WebChromeClientMethod.ONRECEIVEDICON_WEBVIEW_BITMAP;
            case "public void android.webkit.WebChromeClient.onReceivedTouchIconUrl("
                    + "android.webkit.WebView,java.lang.String,boolean)":
                return WebChromeClientMethod.ONRECEIVEDTOUCHICONURL_WEBVIEW_STRING_BOOLEAN;
            case "public void android.webkit.WebChromeClient.onShowCustomView("
                    + "android.view.View,android.webkit.WebChromeClient$CustomViewCallback)":
                return WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_CUSTOMVIEWCALLBACK;
            case "public void android.webkit.WebChromeClient.onShowCustomView("
                    + "android.view.View,int,android.webkit.WebChromeClient$CustomViewCallback)":
                return WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_INT_CUSTOMVIEWCALLBACK;
            case "public void" + " android.webkit.WebChromeClient.onHideCustomView()":
                return WebChromeClientMethod.ONHIDECUSTOMVIEW_;
            case "public boolean android.webkit.WebChromeClient.onCreateWindow("
                    + "android.webkit.WebView,boolean,boolean,android.os.Message)":
                return WebChromeClientMethod.ONCREATEWINDOW_WEBVIEW_BOOLEAN_BOOLEAN_MESSAGE;
            case "public void android.webkit.WebChromeClient.onRequestFocus("
                    + "android.webkit.WebView)":
                return WebChromeClientMethod.ONREQUESTFOCUS_WEBVIEW;
            case "public void android.webkit.WebChromeClient.onCloseWindow("
                    + "android.webkit.WebView)":
                return WebChromeClientMethod.ONCLOSEWINDOW_WEBVIEW;
            case "public boolean android.webkit.WebChromeClient.onJsAlert("
                    + "android.webkit.WebView,java.lang.String,java.lang.String,"
                    + "android.webkit.JsResult)":
                return WebChromeClientMethod.ONJSALERT_WEBVIEW_STRING_STRING_JSRESULT;
            case "public boolean android.webkit.WebChromeClient.onJsConfirm("
                    + "android.webkit.WebView,java.lang.String,java.lang.String,"
                    + "android.webkit.JsResult)":
                return WebChromeClientMethod.ONJSCONFIRM_WEBVIEW_STRING_STRING_JSRESULT;
            case "public boolean android.webkit.WebChromeClient.onJsPrompt("
                    + "android.webkit.WebView,java.lang.String,java.lang.String,java.lang.String,"
                    + "android.webkit.JsPromptResult)":
                return WebChromeClientMethod.ONJSPROMPT_WEBVIEW_STRING_STRING_STRING_JSPROMPTRESULT;
            case "public boolean android.webkit.WebChromeClient.onJsBeforeUnload("
                    + "android.webkit.WebView,java.lang.String,java.lang.String,"
                    + "android.webkit.JsResult)":
                return WebChromeClientMethod.ONJSBEFOREUNLOAD_WEBVIEW_STRING_STRING_JSRESULT;
            case "public void android.webkit.WebChromeClient.onExceededDatabaseQuota("
                    + "java.lang.String,java.lang.String,long,long,long,"
                    + "android.webkit.WebStorage$QuotaUpdater)":
                return WebChromeClientMethod
                        .ONEXCEEDEDDATABASEQUOTA_STRING_STRING_LONG_LONG_LONG_QUOTAUPDATER;
            case "public void android.webkit.WebChromeClient.onReachedMaxAppCacheSize("
                    + "long,long,android.webkit.WebStorage$QuotaUpdater)":
                return WebChromeClientMethod.ONREACHEDMAXAPPCACHESIZE_LONG_LONG_QUOTAUPDATER;
            case "public void android.webkit.WebChromeClient.onGeolocationPermissionsShowPrompt("
                    + "java.lang.String,android.webkit.GeolocationPermissions$Callback)":
                return WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSSHOWPROMPT_STRING_CALLBACK;
            case "public void"
                    + " android.webkit.WebChromeClient.onGeolocationPermissionsHidePrompt()":
                return WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSHIDEPROMPT_;
            case "public void android.webkit.WebChromeClient.onPermissionRequest("
                    + "android.webkit.PermissionRequest)":
                return WebChromeClientMethod.ONPERMISSIONREQUEST_PERMISSIONREQUEST;
            case "public void android.webkit.WebChromeClient.onPermissionRequestCanceled("
                    + "android.webkit.PermissionRequest)":
                return WebChromeClientMethod.ONPERMISSIONREQUESTCANCELED_PERMISSIONREQUEST;
            case "public boolean" + " android.webkit.WebChromeClient.onJsTimeout()":
                return WebChromeClientMethod.ONJSTIMEOUT_;
            case "public void android.webkit.WebChromeClient.onConsoleMessage("
                    + "java.lang.String,int,java.lang.String)":
                return WebChromeClientMethod.ONCONSOLEMESSAGE_STRING_INT_STRING;
            case "public boolean android.webkit.WebChromeClient.onConsoleMessage("
                    + "android.webkit.ConsoleMessage)":
                return WebChromeClientMethod.ONCONSOLEMESSAGE_CONSOLEMESSAGE;
            case "public android.graphics.Bitmap"
                    + " android.webkit.WebChromeClient.getDefaultVideoPoster()":
                return WebChromeClientMethod.GETDEFAULTVIDEOPOSTER_;
            case "public android.view.View"
                    + " android.webkit.WebChromeClient.getVideoLoadingProgressView()":
                return WebChromeClientMethod.GETVIDEOLOADINGPROGRESSVIEW_;
            case "public void android.webkit.WebChromeClient.getVisitedHistory("
                    + "android.webkit.ValueCallback)":
                return WebChromeClientMethod.GETVISITEDHISTORY_VALUECALLBACK;
            case "public boolean android.webkit.WebChromeClient.onShowFileChooser("
                    + "android.webkit.WebView,android.webkit.ValueCallback,"
                    + "android.webkit.WebChromeClient$FileChooserParams)":
                return WebChromeClientMethod
                        .ONSHOWFILECHOOSER_WEBVIEW_VALUECALLBACK_FILECHOOSERPARAMS;
            case "public void android.webkit.WebChromeClient.openFileChooser("
                    + "android.webkit.ValueCallback,java.lang.String,java.lang.String)":
                return WebChromeClientMethod.OPENFILECHOOSER_VALUECALLBACK_STRING_STRING;
            case "public void android.webkit.WebChromeClient.setupAutoFill(android.os.Message)":
                return WebChromeClientMethod.SETUPAUTOFILL_MESSAGE;
            default:
                return WebChromeClientMethod.UNKNOWN;
        }
    }

    /**
     * Allowlist of WebChromeClient methods that should be logged
     *
     * @param method Method from WebViewClient
     * @return true if the method should be logged
     */
    private static boolean shouldLogWebChromeClientMethod(Method method) {
        @WebChromeClientMethod int methodEnum = toWebChromeClientMethodEnum(method);
        return switch (methodEnum) {
            case
                    // Include COUNT and UNKNOWN to have all values present
                    WebChromeClientMethod.COUNT,
                    WebChromeClientMethod.UNKNOWN,
                    // The following values are marked as @removed in the API and should
                    // not be accessed
                    WebChromeClientMethod.ONREACHEDMAXAPPCACHESIZE_LONG_LONG_QUOTAUPDATER -> false;

            case WebChromeClientMethod.ONPROGRESSCHANGED_WEBVIEW_INT,
                    WebChromeClientMethod.ONRECEIVEDTITLE_WEBVIEW_STRING,
                    WebChromeClientMethod.ONRECEIVEDICON_WEBVIEW_BITMAP,
                    WebChromeClientMethod.ONRECEIVEDTOUCHICONURL_WEBVIEW_STRING_BOOLEAN,
                    WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_CUSTOMVIEWCALLBACK,
                    WebChromeClientMethod.ONSHOWCUSTOMVIEW_VIEW_INT_CUSTOMVIEWCALLBACK,
                    WebChromeClientMethod.ONHIDECUSTOMVIEW_,
                    WebChromeClientMethod.ONCREATEWINDOW_WEBVIEW_BOOLEAN_BOOLEAN_MESSAGE,
                    WebChromeClientMethod.ONREQUESTFOCUS_WEBVIEW,
                    WebChromeClientMethod.ONCLOSEWINDOW_WEBVIEW,
                    WebChromeClientMethod.ONJSALERT_WEBVIEW_STRING_STRING_JSRESULT,
                    WebChromeClientMethod.ONJSCONFIRM_WEBVIEW_STRING_STRING_JSRESULT,
                    WebChromeClientMethod.ONJSPROMPT_WEBVIEW_STRING_STRING_STRING_JSPROMPTRESULT,
                    WebChromeClientMethod.ONJSBEFOREUNLOAD_WEBVIEW_STRING_STRING_JSRESULT,
                    WebChromeClientMethod
                            .ONEXCEEDEDDATABASEQUOTA_STRING_STRING_LONG_LONG_LONG_QUOTAUPDATER,
                    WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSSHOWPROMPT_STRING_CALLBACK,
                    WebChromeClientMethod.ONGEOLOCATIONPERMISSIONSHIDEPROMPT_,
                    WebChromeClientMethod.ONPERMISSIONREQUEST_PERMISSIONREQUEST,
                    WebChromeClientMethod.ONPERMISSIONREQUESTCANCELED_PERMISSIONREQUEST,
                    WebChromeClientMethod.ONJSTIMEOUT_,
                    WebChromeClientMethod.ONCONSOLEMESSAGE_STRING_INT_STRING,
                    WebChromeClientMethod.ONCONSOLEMESSAGE_CONSOLEMESSAGE,
                    WebChromeClientMethod.GETDEFAULTVIDEOPOSTER_,
                    WebChromeClientMethod.GETVIDEOLOADINGPROGRESSVIEW_,
                    WebChromeClientMethod.GETVISITEDHISTORY_VALUECALLBACK,
                    WebChromeClientMethod.ONSHOWFILECHOOSER_WEBVIEW_VALUECALLBACK_FILECHOOSERPARAMS,
                    WebChromeClientMethod.OPENFILECHOOSER_VALUECALLBACK_STRING_STRING,
                    WebChromeClientMethod.SETUPAUTOFILL_MESSAGE -> true;
            default -> false; // Just return false if we get an unknown value.
        };
    }

    public static void logWebViewClientImplementation(@NonNull WebViewClient client) {
        logOverriddenImplementation(
                WebViewClient.class,
                client,
                ApiImplementationLogger::shouldLogWebViewClientMethod,
                method ->
                        RecordHistogram.recordEnumeratedHistogram(
                                "Android.WebView.ApiCall.Overridden.WebViewClient",
                                toWebViewClientMethodEnum(method),
                                WebViewClientMethod.COUNT),
                overridden ->
                        RecordHistogram.recordCount100Histogram(
                                "Android.WebView.ApiCall.Overridden.WebViewClient.Count",
                                overridden));
    }

    public static void logWebChromeClientImplementation(@NonNull WebChromeClient client) {
        logOverriddenImplementation(
                WebChromeClient.class,
                client,
                ApiImplementationLogger::shouldLogWebChromeClientMethod,
                method ->
                        RecordHistogram.recordEnumeratedHistogram(
                                "Android.WebView.ApiCall.Overridden.WebChromeClient",
                                toWebChromeClientMethodEnum(method),
                                WebChromeClientMethod.COUNT),
                overridden ->
                        RecordHistogram.recordCount100Histogram(
                                "Android.WebView.ApiCall.Overridden.WebChromeClient.Count",
                                overridden));
    }

    private static <T> void logOverriddenImplementation(
            Class<T> baseClass,
            T implementation,
            Predicate<Method> methodFilter,
            Consumer<Method> histogramRecorder,
            IntConsumer countHistogramRecorder) {
        Method[] declaredMethods = baseClass.getDeclaredMethods();
        int overriddenMethods = 0;
        for (Method method : declaredMethods) {
            if (!methodFilter.test(method)) {
                continue;
            }
            try {
                Method implementedMethod =
                        implementation
                                .getClass()
                                .getMethod(method.getName(), method.getParameterTypes());
                if (!baseClass.equals(implementedMethod.getDeclaringClass())) {
                    overriddenMethods++;
                    histogramRecorder.accept(method);
                }
            } catch (NoSuchMethodException e) {
                // This is highly unlikely to ever happen, as that would mean the implementation
                // class is missing methods from it's superclass.
                Log.d(
                        TAG,
                        "Unable to find method %s on class %s",
                        method.toString(),
                        implementation.getClass().toString());
            }
        }
        countHistogramRecorder.accept(overriddenMethods);
    }
}
