// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.model_execution.ExecutionResult;
import org.chromium.chrome.browser.model_execution.ExecutionResult.ExecutionError;
import org.chromium.chrome.browser.model_execution.ModelExecutionFeature;
import org.chromium.chrome.browser.model_execution.ModelExecutionManager;
import org.chromium.chrome.browser.model_execution.ModelExecutionSession;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.Delegate;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.PageInfoContents;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.Optional;

/**
 * Controls the flow of sharing page info.
 *
 * <p>This class is a singleton because at any point there can only exist one sharing flow.
 */
public class PageInfoSharingControllerImpl implements PageInfoSharingController {

    private static class PageInfoSharingRequest implements Delegate {

        private final Tab mTab;
        private final ChromeOptionShareCallback mChromeOptionShareCallback;
        private final Runnable mDestroyCallback;
        private final ObservableSupplierImpl<PageInfoContents> mPageInfoSupplier;
        private final DestroyChecker mDestroyChecker = new DestroyChecker();

        public PageInfoSharingRequest(
                @NonNull Tab tab,
                @NonNull ChromeOptionShareCallback chromeOptionShareCallback,
                @NonNull ObservableSupplierImpl<PageInfoContents> pageInfoSupplier,
                @NonNull Runnable destroyCallback) {
            mTab = tab;
            mChromeOptionShareCallback = chromeOptionShareCallback;
            mDestroyCallback = destroyCallback;
            mPageInfoSupplier = pageInfoSupplier;
        }

        @Override
        public void onAccept() {
            if (!mPageInfoSupplier.hasValue() || mDestroyChecker.isDestroyed()) return;

            var pageInfo = mPageInfoSupplier.get();
            var chromeShareExtras =
                    new ChromeShareExtras.Builder()
                            .setDetailedContentType(DetailedContentType.PAGE_INFO)
                            .build();

            if (!TextUtils.isEmpty(pageInfo.errorMessage)
                    || pageInfo.isLoading
                    || TextUtils.isEmpty(pageInfo.resultContents)) return;

            ShareParams shareParams =
                    new ShareParams.Builder(
                                    mTab.getWindowAndroid(),
                                    mTab.getTitle(),
                                    mTab.getUrl().getSpec())
                            .setText(pageInfo.resultContents)
                            .build();

            mChromeOptionShareCallback.showShareSheet(
                    shareParams, chromeShareExtras, SystemClock.elapsedRealtime());

            destroy();
        }

        @Override
        public void onCancel() {
            destroy();
        }

        @Override
        public void onPositiveFeedback() {}

        @Override
        public void onNegativeFeedback() {}

        @Override
        public ObservableSupplier<PageInfoContents> getContentSupplier() {
            return mPageInfoSupplier;
        }

        private void destroy() {
            mDestroyChecker.destroy();
            mDestroyCallback.run();
        }
    }

    private ObservableSupplierImpl<PageInfoContents> mCurrentRequestInfoSupplier;
    private ModelExecutionSession mSession;
    private static PageInfoSharingController sInstance;

    public static PageInfoSharingController getInstance() {
        if (sInstance == null) {
            sInstance = new PageInfoSharingControllerImpl();
        }

        return sInstance;
    }

    private PageInfoSharingControllerImpl() {}

    static void resetForTesting() {
        sInstance = null;
    }

    public static void setInstanceForTesting(PageInfoSharingController instanceForTesting) {
        sInstance = instanceForTesting;
        ResettersForTesting.register(PageInfoSharingControllerImpl::resetForTesting);
    }

    /** Implementation of {@code PageInfoSharingController} */
    @Override
    public void initialize() {
        assert mSession == null : "initialize() should be called just once";
        mSession = new ModelExecutionManager().createSession(ModelExecutionFeature.PAGE_INFO);
    }

    /** Implementation of {@code PageInfoSharingController} */
    @Override
    public boolean isAvailableForTab(Tab tab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_PAGE_INFO)) return false;

        if (mSession == null || !mSession.isAvailable()) return false;

        if (mCurrentRequestInfoSupplier != null) return false;

        if (tab == null || tab.getUrl() == null) return false;

        if (!UrlUtilities.isHttpOrHttps(tab.getUrl())) return false;

        if (!PageInfoSharingBridge.doesProfileSupportPageInfo(tab.getProfile())) return false;

        if (!PageInfoSharingBridge.doesTabSupportPageInfo(tab)) return false;

        return true;
    }

    /** Implementation of {@code PageInfoSharingController} */
    @Override
    public void sharePageInfo(
            Context context,
            BottomSheetController bottomSheetController,
            ChromeOptionShareCallback chromeOptionShareCallback,
            Tab tab) {
        if (!isAvailableForTab(tab)) return;

        mCurrentRequestInfoSupplier = new ObservableSupplierImpl<>();
        PageInfoSharingRequest request =
                new PageInfoSharingRequest(
                        tab,
                        chromeOptionShareCallback,
                        mCurrentRequestInfoSupplier,
                        this::onRequestDestroyed);

        PageInfoBottomSheetCoordinator uiCoordinator =
                new PageInfoBottomSheetCoordinator(context, request, bottomSheetController);
        uiCoordinator.requestShowContent();

        InnerTextBridge.getInnerText(tab.getWebContents().getMainFrame(), this::onTabTextReceived);
    }

    public void setModelExecutionSessionForTesting(ModelExecutionSession modelExecutionSession) {
        var oldValue = mSession;
        mSession = modelExecutionSession;
        ResettersForTesting.register(() -> mSession = oldValue);
    }

    private void onTabTextReceived(Optional<String> tabText) {
        if (tabText.isEmpty()) {
            // TODO(salg): Convert error strings into resources.
            mCurrentRequestInfoSupplier.set(
                    new PageInfoContents("Error while extracting page text"));
            return;
        }

        if (TextUtils.isEmpty(tabText.get())) {
            // TODO(salg): Convert error strings into resources.
            mCurrentRequestInfoSupplier.set(new PageInfoContents("Page has no text"));
            return;
        }

        StringBuilder receivedText = new StringBuilder();
        // See javadocs for ModelExecutionSession.executeModel() for details on how this callback
        // gets invoked.
        mSession.executeModel(
                tabText.get(),
                new Callback<ExecutionResult>() {
                    @Override
                    public void onResult(ExecutionResult result) {
                        ThreadUtils.postOnUiThread(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        if (mCurrentRequestInfoSupplier == null) return;

                                        if (result.getErrorCode().isPresent()) {
                                            if (result.getErrorCode().get()
                                                    == ExecutionError.FILTERED) {
                                                // TODO(salg): Convert error strings into resources.
                                                mCurrentRequestInfoSupplier.set(
                                                        new PageInfoContents("Filtered"));
                                            } else {
                                                // TODO(salg): Convert error strings into resources.
                                                mCurrentRequestInfoSupplier.set(
                                                        new PageInfoContents("Error"));
                                            }
                                        } else if (!result.isCompleteResult()) {
                                            receivedText.append(result.getResponse());
                                            mCurrentRequestInfoSupplier.set(
                                                    new PageInfoContents(
                                                            receivedText.toString(), true));
                                        } else {
                                            mCurrentRequestInfoSupplier.set(
                                                    new PageInfoContents(
                                                            result.getResponse(), false));
                                        }
                                    }
                                });
                    }
                });
    }

    @Override
    public void shareWithoutPageInfo(ChromeOptionShareCallback chromeOptionShareCallback, Tab tab) {
        ShareParams shareParams =
                new ShareParams.Builder(
                                tab.getWindowAndroid(), tab.getTitle(), tab.getUrl().getSpec())
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build();
        chromeOptionShareCallback.showShareSheet(
                shareParams, chromeShareExtras, SystemClock.elapsedRealtime());
    }

    private void onRequestDestroyed() {
        mCurrentRequestInfoSupplier = null;
    }
}
