// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.os.Handler;
import android.support.annotation.DrawableRes;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.ErrorObserver;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * PaymentHandlerToolbar mediator, which is responsible for receiving events from the view and
 * notifies the backend (the coordinator).
 */
/* package */ class PaymentHandlerToolbarMediator extends WebContentsObserver {
    // Abbreviated for the length limit.
    private static final String TAG = "PaymentHandlerTb";
    /** The delay (four video frames - for 60Hz) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;

    private final PropertyModel mModel;
    private final ErrorObserver mErrorObserver;
    /** The handler to delay hiding the progress bar. */
    private Handler mHideProgressBarHandler;
    /** Postfixed with "Ref" to distinguish from mWebContent in WebContentsObserver. */
    private final WebContents mWebContentsRef;

    /**
     * Build a new mediator that handle events from outside the payment handler toolbar component.
     * @param model The {@link PaymentHandlerToolbarProperties} that holds all the view state for
     *         the payment handler toolbar component.
     * @param hider The callback to clean up the {@link ErrorObserver} when the sheet is
     *         hidden.
     * @param webContents The web-contents that loads the payment app.
     */
    /* package */ PaymentHandlerToolbarMediator(
            PropertyModel model, WebContents webContents, ErrorObserver errorObserver) {
        super(webContents);
        mWebContentsRef = webContents;
        mModel = model;
        mErrorObserver = errorObserver;
    }

    // WebContentsObserver:
    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        // Hides the Progress Bar after a delay to make sure it is rendered for at least
        // a few frames, otherwise its completion won't be visually noticeable.
        mHideProgressBarHandler = new Handler();
        mHideProgressBarHandler.postDelayed(() -> {
            mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
            mHideProgressBarHandler = null;
        }, HIDE_PROGRESS_BAR_DELAY_MS);
        return;
    }

    @Override
    public void didFailLoad(
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
    }

    @Override
    public void didFinishNavigation(NavigationHandle navigation) {
        if (navigation.hasCommitted() && navigation.isInMainFrame()) {
            String url = navigation.getUrl();
            String origin = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url);
            try {
                mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
                mModel.set(PaymentHandlerToolbarProperties.ORIGIN, new URI(origin));
            } catch (URISyntaxException e) {
                Log.e(TAG, "Failed to instantiate URI with the origin \"%s\", whose url is \"%s\".",
                        origin, url);
                mErrorObserver.onError();
                return;
            }
        }
    }

    @Override
    public void titleWasSet(String title) {
        mModel.set(PaymentHandlerToolbarProperties.TITLE, title);
    }

    @Override
    public void loadProgressChanged(float progress) {
        if (progress == 1.0) return;
        // If the load restarts when the progress bar is waiting to hide, cancel the handler
        // callbacks.
        if (mHideProgressBarHandler != null) {
            mHideProgressBarHandler.removeCallbacksAndMessages(null);
            mHideProgressBarHandler = null;
        }
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true);
        mModel.set(PaymentHandlerToolbarProperties.LOAD_PROGRESS, progress);
    }

    @DrawableRes
    private static int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.WARNING:
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_not_secure_warning;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
            case ConnectionSecurityLevel.EV_SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }

    @Override
    public void didChangeVisibleSecurityState() {
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContentsRef);
        mModel.set(PaymentHandlerToolbarProperties.SECURITY_ICON,
                getSecurityIconResource(securityLevel));
    }
}
