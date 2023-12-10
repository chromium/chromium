// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.app.Activity;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarMediator.PaymentHandlerToolbarMediatorDelegate;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * PaymentHandlerToolbar coordinator, which owns the component overall, i.e., creates other objects
 * in the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerToolbarCoordinator implements PaymentHandlerToolbarMediatorDelegate {
    private final WebContents mWebContents;
    private final Activity mActivity;
    private final boolean mIsSmallDevice;
    private final PropertyModel mModel;
    private final PaymentHandlerToolbarView mToolbarView;
    private final PaymentHandlerToolbarMediator mMediator;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    /** Observer for the error of the payment handler toolbar. */
    public interface PaymentHandlerToolbarObserver {
        /** Called when the close button is clicked. */
        void onToolbarCloseButtonClicked();
    }

    /**
     * Constructs the payment-handler toolbar component coordinator.
     * @param activity The main activity.
     * @param webContents The {@link WebContents} of the payment handler app.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)".
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     */
    public PaymentHandlerToolbarCoordinator(
            @NonNull Activity activity,
            @NonNull WebContents webContents,
            @NonNull GURL url,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        assert activity != null;
        assert webContents != null;
        assert url != null;
        mWebContents = webContents;
        mActivity = activity;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        int defaultSecurityLevel = ConnectionSecurityLevel.NONE;
        mModel =
                new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS)
                        .with(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true)
                        .with(
                                PaymentHandlerToolbarProperties.LOAD_PROGRESS,
                                PaymentHandlerToolbarMediator.MINIMUM_LOAD_PROGRESS)
                        .with(
                                PaymentHandlerToolbarProperties.SECURITY_ICON,
                                getSecurityIconResource(defaultSecurityLevel))
                        .with(
                                PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION,
                                getSecurityIconContentDescription(defaultSecurityLevel))
                        .with(PaymentHandlerToolbarProperties.URL, url)
                        .with(
                                PaymentHandlerToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK,
                                this::showPageInfoDialog)
                        .build();
        mIsSmallDevice = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        mMediator = new PaymentHandlerToolbarMediator(mModel, webContents, /* delegate= */ this);
        mToolbarView = new PaymentHandlerToolbarView(mActivity);
        webContents.addObserver(mMediator);
        PropertyModelChangeProcessor.create(
                mModel, mToolbarView, PaymentHandlerToolbarViewBinder::bind);
    }

    /** Set a callback for the close button's onclick event. */
    public void setCloseButtonOnClickCallback(Runnable callback) {
        mModel.set(PaymentHandlerToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK, callback);
    }

    /** @return The height of the toolbar in px. */
    public int getToolbarHeightPx() {
        return mToolbarView.getToolbarHeightPx();
    }

    /** @return The height of the toolbar shadow height in px. */
    public int getShadowHeightPx() {
        return mToolbarView.getShadowHeightPx();
    }

    /** @return The toolbar of the PaymentHandler. */
    public View getView() {
        return mToolbarView.getView();
    }

    /** Simulates a click on the security icon of the payment handler toolbar. */
    public void clickSecurityIconForTest() {
        mToolbarView.mSecurityIconView.performClick();
    }

    /** Simulates a click on the close button of the payment handler toolbar. */
    public void clickCloseButtonForTest() {
        mToolbarView.mCloseButton.performClick();
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    public @ConnectionSecurityLevel int getSecurityLevel() {
        return SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    public @DrawableRes int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        return SecurityStatusIcon.getSecurityIconResource(
                securityLevel,
                mIsSmallDevice,
                /* skipIconForNeutralState= */ false,
                /* useUpdatedConnectionSecurityIndicators= */ false);
    }

    // Implement PaymentHandlerToolbarMediatorDelegate.
    @Override
    public String getSecurityIconContentDescription(@ConnectionSecurityLevel int securityLevel) {
        int contentDescriptionRes =
                SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(securityLevel);
        return mActivity.getResources().getString(contentDescriptionRes);
    }

    private void showPageInfoDialog() {
        // When creating the {@link ChromePageInfoControllerDelegate} here, we don't need
        // storeInfoActionHandlerSupplier or ephemeralTabCoordinatorSupplier and don't show
        // "store info" row because this UI is already in a bottom sheet and clicking "store info"
        // row would trigger another bottom sheet.
        PageInfoController.show(
                mActivity,
                mWebContents,
                null,
                PageInfoController.OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(
                        mActivity,
                        mWebContents,
                        mModalDialogManagerSupplier,
                        /* offlinePageLoadUrlDelegate= */ new OfflinePageUtils
                                .WebContentsOfflinePageLoadUrlDelegate(mWebContents),
                        /* storeInfoActionHandlerSupplier= */ null,
                        /* ephemeralTabCoordinatorSupplier= */ null,
                        ChromePageInfoHighlight.noHighlight(),
                        null),
                ChromePageInfoHighlight.noHighlight());
    }
}
