// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.ActivityManager.AppTask;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.media.document_picture_in_picture_header.DocumentPictureInPictureHeaderCoordinator;
import org.chromium.chrome.browser.media.document_picture_in_picture_header.DocumentPictureInPictureHeaderDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils.WebContentsOfflinePageLoadUrlDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.toolbar.AppThemeColorProvider;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

@NullMarked
public class DocumentPictureInPictureActivity extends AsyncInitializationActivity
        implements DocumentPictureInPictureHeaderDelegate {
    private static final String TAG = "DocumentPiPActivity";
    public static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WebContents";
    public static final String WINDOW_OPTIONS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WindowOptions";
    private WebContents mWebContents;
    private WebContents mParentWebContents;
    private Tab mInitiatorTab;
    private @MonotonicNonNull ThinWebView mThinWebView;
    private @MonotonicNonNull TabObserver mInitiatorTabObserver;
    private @MonotonicNonNull PictureInPictureWindowOptions mWindowOptions;
    private @MonotonicNonNull AppHeaderCoordinator mAppHeaderCoordinator;
    private @MonotonicNonNull DocumentPictureInPictureHeaderCoordinator mHeaderCoordinator;
    private @MonotonicNonNull AppThemeColorProvider mAppThemeColorProvider;

    private static @Nullable WebContents sWebContentsForTesting;
    // TODO(crbug.com/481216447): Remove this testing bypass once CI supports Android B (API 36).
    private static boolean sIgnoreSdkVersionForTesting;

    @Override
    protected void onPreCreate() {
        super.onPreCreate();

        Intent intent = getIntent();
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        WebContents webContents =
                sWebContentsForTesting != null
                        ? sWebContentsForTesting
                        : intent.getParcelableExtra(WEB_CONTENTS_KEY);
        if (webContents == null) {
            Log.e(TAG, "WebContents is null, finishing.");
            finish();
            return;
        }

        mWebContents = webContents;
        WebContents parentWebContents = mWebContents.getDocumentPictureInPictureOpener();
        mInitiatorTab = TabUtils.fromWebContents(parentWebContents);
        if (parentWebContents == null || TabUtils.getActivity(mInitiatorTab) == null) {
            Log.e(TAG, "Parent web contents or initiator tab is null, finishing.");
            finish();
            return;
        }
        mParentWebContents = parentWebContents;

        Bundle windowOptionsBundle = intent.getBundleExtra(WINDOW_OPTIONS_KEY);
        if (windowOptionsBundle == null) {
            Log.e(TAG, "Window options bundle is null, finishing.");
            finish();
            return;
        }
        mWindowOptions = new PictureInPictureWindowOptions(windowOptionsBundle);

        goIntoPinnedMode();
    }

    /**
     * @return Whether the document pip WebContents and the initiator tab are both initialized.
     */
    @EnsuresNonNullIf({"mWebContents", "mInitiatorTab", "mParentWebContents"})
    private boolean isContentsInitialized() {
        return mWebContents != null && mInitiatorTab != null && mParentWebContents != null;
    }

    @Override
    @SuppressLint("NewAPI")
    public void onStart() {
        super.onStart();
        assert isContentsInitialized();

        DocumentPictureInPictureActivityJni.get().onActivityStart(mParentWebContents, mWebContents);

        mInitiatorTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onClosingStateChanged(Tab tab, boolean closing) {
                        if (closing) {
                            finish();
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (tab.isClosing()) {
                            finish();
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        finish();
                    }
                };
        mInitiatorTab.addObserver(mInitiatorTabObserver);

        // EdgeToEdgeStateProvider is set in ChromeBaseAppCompatActivity#onCreate.
        var edgeToEdgeStateProvider = getEdgeToEdgeStateProvider();
        assert edgeToEdgeStateProvider != null;

        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        this,
                        getWindow().getDecorView().getRootView(),
                        new BrowserStateBrowserControlsVisibilityDelegate(
                                ObservableSuppliers.alwaysFalse()),
                        getInsetObserver(),
                        getLifecycleDispatcher(),
                        getSavedInstanceState(),
                        getPersistentInstanceState(),
                        edgeToEdgeStateProvider,
                        null);

        mAppThemeColorProvider =
                new AppThemeColorProvider(this, getLifecycleDispatcher(), mAppHeaderCoordinator);
        mAppThemeColorProvider.onIncognitoStateChanged(mInitiatorTab.isIncognitoBranded());
    }

    private void goIntoPinnedMode() {
        if (!sIgnoreSdkVersionForTesting && Build.VERSION.SDK_INT < Build.VERSION_CODES.BAKLAVA) {
            Log.e(TAG, "SDK version is too low, minimum required is 36.");
            finish();
            return;
        }

        final AconfigFlaggedApiDelegate aconfigFlaggedApiDelegate =
                AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.e(TAG, "AconfigFlaggedApiDelegate is null");
            finish();
            return;
        }

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(this, getTaskId());
        if (appTask == null) {
            Log.e(TAG, "AppTask is null");
            finish();
            return;
        }

        aconfigFlaggedApiDelegate
                .requestPinnedWindowingLayer(appTask, getMainExecutor())
                .then(
                        CallbackUtils.emptyCallback(),
                        (e) -> {
                            Log.e(
                                    TAG,
                                    "Failed to request pinned windowing layer."
                                            + (e == null ? "" : e.getMessage()));
                            finish();
                        });
    }

    @Override
    public void initializeCompositor() {
        ActivityWindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) {
            windowAndroid = createWindowAndroid();
        }
        ContentView contentView = ContentView.createContentView(this, mWebContents);
        mThinWebView = ThinWebViewFactory.create(this, new ThinWebViewConstraints(), windowAndroid);
        mThinWebView.attachWebContents(
                mWebContents, contentView, new DocumentPictureInPictureWebContentsDelegate());
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        View rootLayout =
                getLayoutInflater().inflate(R.layout.document_picture_in_picture_main_layout, null);
        FrameLayout contentLayout =
                rootLayout.findViewById(R.id.document_picture_in_picture_content);
        contentLayout.addView(
                mThinWebView.getView(),
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        setContentView(rootLayout);

        mHeaderCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        findViewById(R.id.document_picture_in_picture_header),
                        assumeNonNull(mAppHeaderCoordinator),
                        assumeNonNull(mAppThemeColorProvider),
                        /* delegate= */ this,
                        !assumeNonNull(mWindowOptions).disallowReturnToOpener,
                        // TODO(crbug.com/479456911): Dynamically set the security level and
                        // malicious content status if they can change in the same document pip
                        // session.
                        SecurityStateModel.getSecurityLevelForWebContents(mParentWebContents),
                        SecurityStateModel.getMaliciousContentStatusForWebContents(mWebContents),
                        mParentWebContents.getVisibleUrl());

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTO_DOC_PIP_PERMISSION_PROMPT_ANDROID)) {
            WebContents webContents = mParentWebContents;
            if (webContents != null
                    && AutoPictureInPicturePermissionController.isAutoPictureInPictureInUse(
                            webContents)) {
                mThinWebView
                        .getView()
                        .post(
                                () ->
                                        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                                                this, mInitiatorTab, this::finish));
            }
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        assert isContentsInitialized();
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {

                    @Override
                    public Profile getOriginalProfile() {
                        return mInitiatorTab.getProfile().getOriginalProfile();
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        if (!mInitiatorTab.getProfile().isOffTheRecord()) {
                            assert !createIfNeeded;
                            return null;
                        }
                        return mInitiatorTab.getProfile();
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* trackOcclusion= */ true);
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        // EdgeToEdgeStateProvider is set in ChromeBaseAppCompatActivity#onCreate.
        assert getEdgeToEdgeStateProvider() != null;

        return new ModalDialogManager(
                new AppModalPresenter(this),
                ModalDialogManager.ModalDialogType.APP,
                getEdgeToEdgeStateProvider().getSupplier(),
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
    }

    @Override
    @SuppressWarnings("NullAway")
    protected final void onDestroy() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }

        if (mInitiatorTabObserver != null && mInitiatorTab != null) {
            mInitiatorTab.removeObserver(mInitiatorTabObserver);
        }

        mInitiatorTab = null;
        mInitiatorTabObserver = null;

        if (mHeaderCoordinator != null) {
            mHeaderCoordinator.destroy();
            mHeaderCoordinator = null;
        }

        if (mAppThemeColorProvider != null) {
            mAppThemeColorProvider.destroy();
            mAppThemeColorProvider = null;
        }

        super.onDestroy();
    }

    @Override
    public void onBackToTab() {
        DocumentPictureInPictureActivityJni.get().onBackToTab();
    }

    @Override
    public void onSecurityIconClicked() {
        // TODO(crbug.com/479732663): Move the click handling to the coordinator.
        PageInfoController.show(
                this,
                mParentWebContents,
                /* contentPublisher= */ null,
                OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(
                        this,
                        mParentWebContents,
                        () -> getModalDialogManagerSupplier().get(),
                        new WebContentsOfflinePageLoadUrlDelegate(mParentWebContents),
                        /* storeInfoActionHandlerSupplier= */ null,
                        /* ephemeralTabCoordinatorSupplier= */ null,
                        ChromePageInfoHighlight.noHighlight(),
                        /* tabCreator= */ null,
                        /* packageName= */ null),
                ChromePageInfoHighlight.noHighlight(),
                Gravity.TOP);
    }

    private class DocumentPictureInPictureWebContentsDelegate extends WebContentsDelegateAndroid {
        @Override
        public void closeContents() {
            finish();
        }

        @Override
        public void openNewTab(
                GURL url,
                String extraHeaders,
                ResourceRequestBody postData,
                int disposition,
                boolean isRendererInitiated) {
            finish();
        }

        @Override
        public void setContentsBounds(WebContents source, Rect bounds) {
            MultiWindowUtils.moveActivityToBounds(DocumentPictureInPictureActivity.this, bounds);
        }
    }

    public static void setWebContentsForTesting(WebContents webContents) {
        sWebContentsForTesting = webContents;
    }

    public static void setIgnoreSdkVersionForTesting(boolean ignore) {
        sIgnoreSdkVersionForTesting = ignore;
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(WebContents parentWebContent, WebContents webContents);

        void onBackToTab();
    }
}
