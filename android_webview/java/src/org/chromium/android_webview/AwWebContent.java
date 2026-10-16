// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslCertificate;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.textclassifier.TextClassifier;
import android.widget.FrameLayout;

import androidx.annotation.UiThread;

import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.gfx.AwDrawFnImpl.DrawFnAccess;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHistory;

import java.util.Map;

/**
 * Represents underlying Chromium web contents state that can survive moving across WebViews.
 *
 * <p>`AwWebContent` can be constructed even before Chromium startup runs.
 *
 * <p>Any methods on this class must only be called on the UI thread.
 */
@UiThread
@NullMarked
public class AwWebContent {
    /**
     * Listener notified when the {@link AwContents} associated with this {@link AwWebContent}
     * changes, or {@code null} when detached or not yet initialized.
     */
    public interface SurfaceBindingListener {
        void onAwContentsChanged(@Nullable AwContents awContents);
    }

    /** The current owner of this {@link AwWebContent}. There is at most one host at a time. */
    public interface ViewHost {
        /**
         * Called when `AwSettings` is available during adoption.
         *
         * <p>`AwContents` owns the `AwSettings` object so in an ideal world, we shouldn't be
         * passing this to the host. But we observed that during `AwContents` construction,
         * depending on the app's implementation, it is possible that there is a call to
         * `WebViewChromium.getSettings()`. Since `AwContents` is not fully initialized at that
         * point, `getSettings()` would return `null`. Therefore, we pass the settings to the host
         * as soon as the settings are available. Refer: b/556721003
         */
        void initSettings(AwSettings settings);

        /**
         * Called when this {@link AwWebContent} is adopted by a new host, before the new host's
         * adoption begins.
         */
        void onDetached();
    }

    private @Nullable AwContents mAwContents;
    private @Nullable ViewHost mCurrentHost;
    private @Nullable SurfaceBindingListener mSurfaceBindingListener;
    private boolean mIsDestroyed;

    /**
     * Creates an {@link AwWebContent} that has already been destroyed.
     *
     * <p>A host whose content has been adopted by another host can swap its {@link AwWebContent} to
     * the returned instance so that any further calls on the old host are ignored, the same way
     * calls on a destroyed WebView are.
     *
     * <p>The returned instance cannot not be adopted.
     *
     * <p>Each call creates a new destroyed {@link AwContents}.
     *
     * <p>Must only be called on the UI thread after Chromium startup has completed.
     *
     * <p>TODO(bewise): Replace this destroyed AwWebContent approach with better state management so
     * that long term we can clean up the default profile when it isn't in use.
     */
    public static AwWebContent createDestroyedAwWebContent(
            AwContents.AwContentsClientFactory clientFactory) {
        ThreadUtils.assertOnUiThread();
        Context appContext = ContextUtils.getApplicationContext();
        AwWebContent destroyedAwWebContent = new AwWebContent();
        destroyedAwWebContent.mAwContents =
                new AwContents(
                        AwBrowserContextStore.getNamedContext(
                                AwBrowserContext.getDefaultContextName(), true),
                        new FrameLayout(appContext),
                        appContext,
                        new NullInternalAccessDelegate(),
                        (canvas, functor) -> {},
                        clientFactory);
        destroyedAwWebContent.mAwContents.destroy();
        destroyedAwWebContent.mIsDestroyed = true;
        return destroyedAwWebContent;
    }

    /** No-op {@link InternalAccessDelegate} for the destroyed {@link AwContents}. */
    private static final class NullInternalAccessDelegate implements InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {}

        @Override
        public int super_getScrollBarStyle() {
            return View.SCROLLBARS_INSIDE_OVERLAY;
        }

        @Override
        public void super_startActivityForResult(Intent intent, int requestCode) {}

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {}

        @Override
        public void overScrollBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                int maxOverScrollX,
                int maxOverScrollY,
                boolean isTouchEvent) {}

        @Override
        public void super_scrollTo(int scrollX, int scrollY) {}

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {}
    }

    public boolean isInitialized() {
        return mAwContents != null;
    }

    /**
     * Returns the {@link AwContents} backing this {@link AwWebContent}, or null if it has not been
     * adopted yet.
     *
     * <p>TODO: Eventually get rid of this method such that holders hold onto `AwWebContent` instead
     * of `AwContents`.
     */
    public @Nullable AwContents getAwContents() {
        return mAwContents;
    }

    public void bindSurface(@Nullable SurfaceBindingListener listener) {
        if (mSurfaceBindingListener == listener) {
            return;
        }
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(null);
        }
        mSurfaceBindingListener = listener;
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(mAwContents);
        }
    }

    /**
     * Returns a new AwContents instance if this is the first time this AwWebContent is being
     * adopted, or the existing instance if this AwWebContent has been adopted before.
     *
     * <p>This method must only be called after Chromium startup has completed.
     *
     * <p>Note for future: This method returning `AwContents` is temporary. Once all callers are
     * migrated to not refer to `AwContents` directly, we will no longer return `AwContents`.
     */
    public AwContents adopt(
            ViewHost host,
            AwBrowserContext browserContext,
            ViewGroup containerView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            DrawFnAccess drawFnAccess,
            AwContents.AwContentsClientFactory clientFactory,
            DependencyFactory dependencyFactory) {
        if (mIsDestroyed) {
            throw new IllegalStateException(
                    "Cannot adopt an AwWebContent instance after destroy() has been called.");
        }

        assert mCurrentHost != host : "Cannot adopt an AwWebContent into the same host twice.";

        if (mCurrentHost != null) {
            mCurrentHost.onDetached();
        }
        mCurrentHost = host;

        if (mAwContents == null) {
            mAwContents =
                    new AwContents(
                            browserContext,
                            containerView,
                            context,
                            internalAccessAdapter,
                            drawFnAccess,
                            clientFactory,
                            host::initSettings,
                            dependencyFactory);
        } else {
            host.initSettings(mAwContents.getSettings());
            mAwContents.adopt(containerView, internalAccessAdapter);
        }

        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(mAwContents);
        }

        return mAwContents;
    }

    // The following methods trigger startup. It waits for Chromium to start before executing the
    // action if startup has not yet completed.

    public void evaluateJavaScript(String script, @Nullable Callback<String> callback) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_EVALUATE_JAVASCRIPT)
                .evaluateJavaScript(script, callback);
    }

    public boolean canGoBack() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_CAN_GO_BACK).canGoBack();
    }

    public boolean canGoForward() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_CAN_GO_FORWARD).canGoForward();
    }

    public boolean canGoBackOrForward(int steps) {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_CAN_GO_BACK_OR_FORWARD)
                .canGoBackOrForward(steps);
    }

    public @Nullable NavigationHistory getNavigationHistory() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_COPY_BACK_FORWARD_LIST)
                .getNavigationHistory();
    }

    public boolean saveState(Bundle outState) {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_SAVE_STATE).saveState(outState);
    }

    public boolean restoreState(Bundle inState) {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_RESTORE_STATE)
                .restoreState(inState);
    }

    public @Nullable String getUrl() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_URL).getUrl();
    }

    public @Nullable String getOriginalUrl() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_ORIGINAL_URL).getOriginalUrl();
    }

    public @Nullable String getTitle() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_TITLE).getTitle();
    }

    public @Nullable Bitmap getFavicon() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_FAVICON).getFavicon();
    }

    public AwContents.HitTestData getLastHitTestResult() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_HIT_TEST_RESULT)
                .getLastHitTestResult();
    }

    public @Nullable Picture capturePicture() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_CAPTURE_PICTURE).capturePicture();
    }

    public float getScale() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_SCALE).getScale();
    }

    public boolean zoomIn() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_ZOOM_IN).zoomIn();
    }

    public boolean zoomOut() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_ZOOM_OUT).zoomOut();
    }

    public void zoomBy(float delta) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_ZOOM_BY).zoomBy(delta);
    }

    public boolean pageUp(boolean top) {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_PAGE_UP).pageUp(top);
    }

    public boolean pageDown(boolean bottom) {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_PAGE_DOWN).pageDown(bottom);
    }

    public void setRendererPriorityPolicy(
            @RendererPriority int rendererRequestedPriority, boolean waivedWhenNotVisible) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_SET_RENDERER_PRIORITY_POLICY)
                .setRendererPriorityPolicy(rendererRequestedPriority, waivedWhenNotVisible);
    }

    @RendererPriority
    public int getRendererRequestedPriority() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_RENDERER_REQUESTED_PRIORITY)
                .getRendererRequestedPriority();
    }

    public boolean getRendererPriorityWaivedWhenNotVisible() {
        return ensureAwContents(
                        StartupCallSite
                                .WEBVIEW_INSTANCE_GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE)
                .getRendererPriorityWaivedWhenNotVisible();
    }

    public void setTextClassifier(@Nullable TextClassifier textClassifier) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_SET_TEXT_CLASSIFIER)
                .setTextClassifier(textClassifier);
    }

    public @Nullable TextClassifier getTextClassifier() {
        return ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_GET_TEXT_CLASSIFIER)
                .getTextClassifier();
    }

    public void extractSmartClipData(int x, int y, int width, int height) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_EXTRACT_SMART_CLIP_DATA)
                .extractSmartClipData(x, y, width, height);
    }

    public void setSmartClipResultHandler(@Nullable Handler resultHandler) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_SET_SMART_CLIP_RESULT_HANDLER)
                .setSmartClipResultHandler(resultHandler);
    }

    public AwPrintDocumentAdapter createPrintDocumentAdapter(String documentName) {
        return new AwPrintDocumentAdapter(
                ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_CREATE_PRINT_DOCUMENT_ADAPTER)
                        .getPdfExporter(),
                documentName);
    }

    public void loadUrl(@Nullable String url) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_LOAD_URL).loadUrl(url);
    }

    public void loadUrl(@Nullable String url, @Nullable Map<String, String> additionalHttpHeaders) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_LOAD_URL_ADDITIONAL_HEADERS)
                .loadUrl(url, additionalHttpHeaders);
    }

    public void postUrl(String url, byte[] postData) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_POST_URL).postUrl(url, postData);
    }

    public void loadData(
            @Nullable String data, @Nullable String mimeType, @Nullable String encoding) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_LOAD_DATA)
                .loadData(data, mimeType, encoding);
    }

    public void loadDataWithBaseURL(
            @Nullable String baseUrl,
            @Nullable String data,
            @Nullable String mimeType,
            @Nullable String encoding,
            @Nullable String historyUrl) {
        ensureAwContents(StartupCallSite.WEBVIEW_INSTANCE_LOAD_DATA_WITH_BASE_URL)
                .loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
    }

    // The following methods don't trigger startup. They queue the action to run once Chromium has
    // started or run the action immediately if Chromium has already started.

    public void goBack() {
        runOrQueueUntilStarted(AwContents::goBack);
    }

    public void goForward() {
        runOrQueueUntilStarted(AwContents::goForward);
    }

    public void goBackOrForward(int steps) {
        runOrQueueUntilStarted(awContents -> awContents.goBackOrForward(steps));
    }

    public void stopLoading() {
        runOrQueueUntilStarted(AwContents::stopLoading);
    }

    public void reload() {
        runOrQueueUntilStarted(AwContents::reload);
    }

    public void onPause() {
        runOrQueueUntilStarted(AwContents::onPause);
    }

    public void onResume() {
        runOrQueueUntilStarted(AwContents::onResume);
    }

    public void pauseTimers() {
        runOrQueueUntilStarted(AwContents::pauseTimers);
    }

    public void resumeTimers() {
        runOrQueueUntilStarted(AwContents::resumeTimers);
    }

    public void setNetworkAvailable(boolean networkUp) {
        runOrQueueUntilStarted(awContents -> awContents.setNetworkAvailable(networkUp));
    }

    public void clearCache(boolean includeDiskFiles) {
        runOrQueueUntilStarted(awContents -> awContents.clearCache(includeDiskFiles));
    }

    public void clearHistory() {
        runOrQueueUntilStarted(AwContents::clearHistory);
    }

    public void clearSslPreferences() {
        runOrQueueUntilStarted(AwContents::clearSslPreferences);
    }

    public void clearView() {
        runOrQueueUntilStarted(AwContents::clearView);
    }

    public void hideAutofillPopup() {
        runOrQueueUntilStarted(AwContents::hideAutofillPopup);
    }

    public void findNext(boolean forward) {
        runOrQueueUntilStarted(awContents -> awContents.findNext(forward));
    }

    public void findAllAsync(@Nullable String searchString) {
        runOrQueueUntilStarted(awContents -> awContents.findAllAsync(searchString));
    }

    public void clearMatches() {
        runOrQueueUntilStarted(AwContents::clearMatches);
    }

    public void documentHasImages(Message message) {
        runOrQueueUntilStarted(awContents -> awContents.documentHasImages(message));
    }

    public void requestFocusNodeHref(@Nullable Message msg) {
        runOrQueueUntilStarted(awContents -> awContents.requestFocusNodeHref(msg));
    }

    public void requestImageRef(@Nullable Message msg) {
        runOrQueueUntilStarted(awContents -> awContents.requestImageRef(msg));
    }

    public void saveWebArchive(
            String basename, boolean autoname, @Nullable Callback<String> callback) {
        runOrQueueUntilStarted(
                awContents -> awContents.saveWebArchive(basename, autoname, callback));
    }

    public void addJavascriptInterface(Object object, String name) {
        runOrQueueUntilStarted(awContents -> awContents.addJavascriptInterface(object, name));
    }

    public void removeJavascriptInterface(String interfaceName) {
        runOrQueueUntilStarted(awContents -> awContents.removeJavascriptInterface(interfaceName));
    }

    public void invokeZoomPicker() {
        runOrQueueUntilStarted(AwContents::invokeZoomPicker);
    }

    public void flingScroll(int velocityX, int velocityY) {
        runOrQueueUntilStarted(awContents -> awContents.flingScroll(velocityX, velocityY));
    }

    // The following methods don't trigger startup. They return a default value if the AwContents
    // hasn't been created yet.

    public @Nullable SslCertificate getCertificate() {
        return mAwContents == null ? null : mAwContents.getCertificate();
    }

    public int getMostRecentProgress() {
        return mAwContents == null ? 100 : mAwContents.getMostRecentProgress();
    }

    public int getContentHeightCss() {
        return mAwContents == null ? 0 : mAwContents.getContentHeightCss();
    }

    public int getContentWidthCss() {
        return mAwContents == null ? 0 : mAwContents.getContentWidthCss();
    }

    public boolean canZoomIn() {
        return mAwContents != null && mAwContents.canZoomIn();
    }

    public boolean canZoomOut() {
        return mAwContents != null && mAwContents.canZoomOut();
    }

    private AwContents ensureAwContents(@StartupCallSite int callSite) {
        StartupController.getInstance().triggerAndWaitForChromiumStarted(callSite);
        assert mAwContents != null;
        return mAwContents;
    }

    /**
     * Runs {@code action} with the adopted {@link AwContents} synchronously if Chromium has
     * started, or queues it to run asynchronously once startup completes. Unlike {@link
     * #ensureAwContents}, this does not trigger startup.
     *
     * <p>Warning: This method may lead to unnecessary allocation and free of an object for the
     * callable in the non-queue case - which may have non-zero performance penalty. Do not use this
     * method for performance critical code, e.g. UI rendering.
     */
    private void runOrQueueUntilStarted(Callback<AwContents> action) {
        StartupController startupController = StartupController.getInstance();
        if (mAwContents != null) {
            action.onResult(mAwContents);
            return;
        }
        startupController
                .getRunQueue()
                .addTask(
                        () -> {
                            assert mAwContents != null;
                            action.onResult(mAwContents);
                        });
    }

    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;
        if (mSurfaceBindingListener != null) {
            mSurfaceBindingListener.onAwContentsChanged(null);
            mSurfaceBindingListener = null;
        }
        mCurrentHost = null;
        if (mAwContents != null) {
            mAwContents.destroy();
        }
    }
}
