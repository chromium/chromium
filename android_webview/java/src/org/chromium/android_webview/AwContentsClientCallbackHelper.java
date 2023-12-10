// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Picture;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.util.concurrent.Callable;

/**
 * This class is responsible for calling certain client callbacks on the UI thread.
 *
 * Most callbacks do no go through here, but get forwarded to AwContentsClient directly. The
 * messages processed here may originate from the IO or UI thread.
 */
@Lifetime.WebView
public class AwContentsClientCallbackHelper {
    /** Interface to tell CallbackHelper to cancel posted callbacks. */
    public static interface CancelCallbackPoller {
        boolean shouldCancelAllCallbacks();
    }

    // TODO(boliu): Consider removing DownloadInfo and LoginRequestInfo by using native
    // MessageLoop to post directly to AwContents.

    private static class DownloadInfo {
        final String mUrl;
        final String mUserAgent;
        final String mContentDisposition;
        final String mMimeType;
        final long mContentLength;

        DownloadInfo(
                String url,
                String userAgent,
                String contentDisposition,
                String mimeType,
                long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
        }
    }

    private static class LoginRequestInfo {
        final String mRealm;
        final String mAccount;
        final String mArgs;

        LoginRequestInfo(String realm, String account, String args) {
            mRealm = realm;
            mAccount = account;
            mArgs = args;
        }
    }

    private static class OnReceivedErrorInfo {
        final AwContentsClient.AwWebResourceRequest mRequest;
        final AwContentsClient.AwWebResourceError mError;

        OnReceivedErrorInfo(
                AwContentsClient.AwWebResourceRequest request,
                AwContentsClient.AwWebResourceError error) {
            mRequest = request;
            mError = error;
        }
    }

    private static class OnSafeBrowsingHitInfo {
        final AwContentsClient.AwWebResourceRequest mRequest;
        final int mThreatType;
        final Callback<AwSafeBrowsingResponse> mCallback;

        OnSafeBrowsingHitInfo(
                AwContentsClient.AwWebResourceRequest request,
                int threatType,
                Callback<AwSafeBrowsingResponse> callback) {
            mRequest = request;
            mThreatType = threatType;
            mCallback = callback;
        }
    }

    private static class OnReceivedHttpErrorInfo {
        final AwContentsClient.AwWebResourceRequest mRequest;
        final WebResourceResponseInfo mResponse;

        OnReceivedHttpErrorInfo(
                AwContentsClient.AwWebResourceRequest request, WebResourceResponseInfo response) {
            mRequest = request;
            mResponse = response;
        }
    }

    private static class DoUpdateVisitedHistoryInfo {
        final String mUrl;
        final boolean mIsReload;

        DoUpdateVisitedHistoryInfo(String url, boolean isReload) {
            mUrl = url;
            mIsReload = isReload;
        }
    }

    private static class OnFormResubmissionInfo {
        final Message mDontResend;
        final Message mResend;

        OnFormResubmissionInfo(Message dontResend, Message resend) {
            mDontResend = dontResend;
            mResend = resend;
        }
    }

    private static final int MSG_ON_LOAD_RESOURCE = 1;
    private static final int MSG_ON_PAGE_STARTED = 2;
    private static final int MSG_ON_DOWNLOAD_START = 3;
    private static final int MSG_ON_RECEIVED_LOGIN_REQUEST = 4;
    private static final int MSG_ON_RECEIVED_ERROR = 5;
    private static final int MSG_ON_NEW_PICTURE = 6;
    private static final int MSG_ON_SCALE_CHANGED_SCALED = 7;
    private static final int MSG_ON_RECEIVED_HTTP_ERROR = 8;
    private static final int MSG_ON_PAGE_FINISHED = 9;
    private static final int MSG_ON_RECEIVED_TITLE = 10;
    private static final int MSG_ON_PROGRESS_CHANGED = 11;
    private static final int MSG_SYNTHESIZE_PAGE_LOADING = 12;
    private static final int MSG_DO_UPDATE_VISITED_HISTORY = 13;
    private static final int MSG_ON_FORM_RESUBMISSION = 14;
    private static final int MSG_ON_SAFE_BROWSING_HIT = 15;

    // Minimum period allowed between consecutive onNewPicture calls, to rate-limit the callbacks.
    private static final long ON_NEW_PICTURE_MIN_PERIOD_MILLIS = 500;
    // Timestamp of the most recent onNewPicture callback.
    private long mLastPictureTime;
    // True when a onNewPicture callback is currenly in flight.
    private boolean mHasPendingOnNewPicture;

    private final AwContentsClient mContentsClient;

    private final Handler mHandler;

    private CancelCallbackPoller mCancelCallbackPoller;

    private class MyHandler extends Handler {
        private MyHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (mCancelCallbackPoller != null && mCancelCallbackPoller.shouldCancelAllCallbacks()) {
                removeCallbacksAndMessages(null);
                return;
            }

            switch (msg.what) {
                case MSG_ON_LOAD_RESOURCE:
                    {
                        final String url = (String) msg.obj;
                        mContentsClient.onLoadResource(url);
                        break;
                    }
                case MSG_ON_PAGE_STARTED:
                    {
                        final String url = (String) msg.obj;
                        mContentsClient.onPageStarted(url);
                        break;
                    }
                case MSG_ON_DOWNLOAD_START:
                    {
                        DownloadInfo info = (DownloadInfo) msg.obj;
                        mContentsClient.onDownloadStart(
                                info.mUrl,
                                info.mUserAgent,
                                info.mContentDisposition,
                                info.mMimeType,
                                info.mContentLength);
                        break;
                    }
                case MSG_ON_RECEIVED_LOGIN_REQUEST:
                    {
                        LoginRequestInfo info = (LoginRequestInfo) msg.obj;
                        mContentsClient.onReceivedLoginRequest(
                                info.mRealm, info.mAccount, info.mArgs);
                        break;
                    }
                case MSG_ON_RECEIVED_ERROR:
                    {
                        OnReceivedErrorInfo info = (OnReceivedErrorInfo) msg.obj;
                        mContentsClient.onReceivedError(info.mRequest, info.mError);
                        break;
                    }
                case MSG_ON_SAFE_BROWSING_HIT:
                    {
                        OnSafeBrowsingHitInfo info = (OnSafeBrowsingHitInfo) msg.obj;
                        mContentsClient.onSafeBrowsingHit(
                                info.mRequest, info.mThreatType, info.mCallback);
                        break;
                    }
                case MSG_ON_NEW_PICTURE:
                    {
                        Picture picture = null;
                        try {
                            if (msg.obj != null) picture = (Picture) ((Callable<?>) msg.obj).call();
                        } catch (Exception e) {
                            throw new RuntimeException("Error getting picture", e);
                        }
                        mContentsClient.onNewPicture(picture);
                        mLastPictureTime = SystemClock.uptimeMillis();
                        mHasPendingOnNewPicture = false;
                        break;
                    }
                case MSG_ON_SCALE_CHANGED_SCALED:
                    {
                        float oldScale = Float.intBitsToFloat(msg.arg1);
                        float newScale = Float.intBitsToFloat(msg.arg2);
                        mContentsClient.onScaleChangedScaled(oldScale, newScale);
                        break;
                    }
                case MSG_ON_RECEIVED_HTTP_ERROR:
                    {
                        OnReceivedHttpErrorInfo info = (OnReceivedHttpErrorInfo) msg.obj;
                        mContentsClient.onReceivedHttpError(info.mRequest, info.mResponse);
                        break;
                    }
                case MSG_ON_PAGE_FINISHED:
                    {
                        final String url = (String) msg.obj;
                        mContentsClient.onPageFinished(url);
                        break;
                    }
                case MSG_ON_RECEIVED_TITLE:
                    {
                        final String title = (String) msg.obj;
                        mContentsClient.onReceivedTitle(title);
                        break;
                    }
                case MSG_ON_PROGRESS_CHANGED:
                    {
                        mContentsClient.onProgressChanged(msg.arg1);
                        break;
                    }
                case MSG_SYNTHESIZE_PAGE_LOADING:
                    {
                        final String url = (String) msg.obj;
                        mContentsClient.onPageStarted(url);
                        mContentsClient.onLoadResource(url);
                        mContentsClient.onProgressChanged(100);
                        mContentsClient.onPageFinished(url);
                        break;
                    }
                case MSG_DO_UPDATE_VISITED_HISTORY:
                    {
                        final DoUpdateVisitedHistoryInfo info =
                                (DoUpdateVisitedHistoryInfo) msg.obj;
                        mContentsClient.doUpdateVisitedHistory(info.mUrl, info.mIsReload);
                        break;
                    }
                case MSG_ON_FORM_RESUBMISSION:
                    {
                        final OnFormResubmissionInfo info = (OnFormResubmissionInfo) msg.obj;
                        mContentsClient.onFormResubmission(info.mDontResend, info.mResend);
                        break;
                    }
                default:
                    throw new IllegalStateException(
                            "AwContentsClientCallbackHelper: unhandled message " + msg.what);
            }
        }
    }

    public AwContentsClientCallbackHelper(Looper looper, AwContentsClient contentsClient) {
        mHandler = new MyHandler(looper);
        mContentsClient = contentsClient;
    }

    // Public for tests.
    public void setCancelCallbackPoller(CancelCallbackPoller poller) {
        mCancelCallbackPoller = poller;
    }

    CancelCallbackPoller getCancelCallbackPoller() {
        return mCancelCallbackPoller;
    }

    public void postOnLoadResource(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_LOAD_RESOURCE, url));
    }

    public void postOnPageStarted(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PAGE_STARTED, url));
    }

    public void postOnDownloadStart(
            String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        DownloadInfo info =
                new DownloadInfo(url, userAgent, contentDisposition, mimeType, contentLength);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_DOWNLOAD_START, info));
    }

    public void postOnReceivedLoginRequest(String realm, String account, String args) {
        LoginRequestInfo info = new LoginRequestInfo(realm, account, args);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_LOGIN_REQUEST, info));
    }

    public void postOnReceivedError(
            AwContentsClient.AwWebResourceRequest request,
            AwContentsClient.AwWebResourceError error) {
        OnReceivedErrorInfo info = new OnReceivedErrorInfo(request, error);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_ERROR, info));
    }

    public void postOnSafeBrowsingHit(
            AwContentsClient.AwWebResourceRequest request,
            int threatType,
            Callback<AwSafeBrowsingResponse> callback) {
        OnSafeBrowsingHitInfo info = new OnSafeBrowsingHitInfo(request, threatType, callback);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_SAFE_BROWSING_HIT, info));
    }

    public void postOnNewPicture(Callable<Picture> pictureProvider) {
        if (mHasPendingOnNewPicture) return;
        mHasPendingOnNewPicture = true;
        long pictureTime =
                java.lang.Math.max(
                        mLastPictureTime + ON_NEW_PICTURE_MIN_PERIOD_MILLIS,
                        SystemClock.uptimeMillis());
        mHandler.sendMessageAtTime(
                mHandler.obtainMessage(MSG_ON_NEW_PICTURE, pictureProvider), pictureTime);
    }

    public void postOnScaleChangedScaled(float oldScale, float newScale) {
        // The float->int->float conversion here is to avoid unnecessary allocations. The
        // documentation states that intBitsToFloat(floatToIntBits(a)) == a for all values of a
        // (except for NaNs which are collapsed to a single canonical NaN, but we don't care for
        // that case).
        mHandler.sendMessage(
                mHandler.obtainMessage(
                        MSG_ON_SCALE_CHANGED_SCALED,
                        Float.floatToIntBits(oldScale),
                        Float.floatToIntBits(newScale)));
    }

    public void postOnReceivedHttpError(
            AwContentsClient.AwWebResourceRequest request, WebResourceResponseInfo response) {
        OnReceivedHttpErrorInfo info = new OnReceivedHttpErrorInfo(request, response);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_HTTP_ERROR, info));
    }

    public void postOnPageFinished(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PAGE_FINISHED, url));
    }

    public void postOnReceivedTitle(String title) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_RECEIVED_TITLE, title));
    }

    public void postOnProgressChanged(int progress) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_PROGRESS_CHANGED, progress, 0));
    }

    public void postSynthesizedPageLoadingForUrlBarUpdate(String url) {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_SYNTHESIZE_PAGE_LOADING, url));
    }

    public void postDoUpdateVisitedHistory(String url, boolean isReload) {
        DoUpdateVisitedHistoryInfo info = new DoUpdateVisitedHistoryInfo(url, isReload);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_DO_UPDATE_VISITED_HISTORY, info));
    }

    public void postOnFormResubmission(Message dontResend, Message resend) {
        OnFormResubmissionInfo info = new OnFormResubmissionInfo(dontResend, resend);
        mHandler.sendMessage(mHandler.obtainMessage(MSG_ON_FORM_RESUBMISSION, info));
    }

    void removeCallbacksAndMessages() {
        mHandler.removeCallbacksAndMessages(null);
    }
}
