// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.os.Handler;

import androidx.annotation.DrawableRes;

import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * PaymentHandlerToolbar mediator, which is responsible for receiving events from the view and
 * notifies the backend (the coordinator).
 */
/* package */ class PaymentHandlerToolbarMediator extends WebContentsObserver {
    /** The delay (four video frames - for 60Hz) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;

    /**
     * The minimum load progress that can be shown when a page is loading. This is not 0 so that
     * it's obvious to the user that something is attempting to load.
     */
    /* package */ static final float MINIMUM_LOAD_PROGRESS = 0.05f;

    private final PropertyModel mModel;

    /** The handler to delay hiding the progress bar. */
    private Handler mHideProgressBarHandler;

    /** Postfix with "Ref" to distinguish from mWebContent in WebContentsObserver. */
    private final WebContents mWebContentsRef;

    private final PaymentHandlerToolbarMediatorDelegate mDelegate;

    /** The delegate of PaymentHandlerToolbarMediator. */
    /* package */ interface PaymentHandlerToolbarMediatorDelegate {
        /** Get the security level of PaymentHandler's WebContents. */
        int getSecurityLevel();

        /**
         * Get the security icon resource for a given security level.
         * @param securityLevel The security level.
         */
        @DrawableRes
        int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel);

        /**
         * Get the content description of the security icon for a given security level.
         * @param securityLevel The security level.
         */
        String getSecurityIconContentDescription(@ConnectionSecurityLevel int securityLevel);
    }

    /**
     * Build a new mediator that handle events from outside the payment handler toolbar component.
     * @param model The {@link PaymentHandlerToolbarProperties} that holds all the view state for
     *         the payment handler toolbar component.
     * @param webContents The web-contents that loads the payment app.
     * @param delegate The delegate of this class.
     */
    /* package */ PaymentHandlerToolbarMediator(
            PropertyModel model,
            WebContents webContents,
            PaymentHandlerToolbarMediatorDelegate delegate) {
        super(webContents);
        mWebContentsRef = webContents;
        mModel = model;
        mDelegate = delegate;
    }

    // WebContentsObserver:
    @Override
    public void didFinishLoadInPrimaryMainFrame(
            GlobalRenderFrameHostId rfhId,
            GURL url,
            boolean isKnownValid,
            @LifecycleState int rfhLifecycleState) {
        if (rfhLifecycleState != LifecycleState.ACTIVE) return;
        // Hides the Progress Bar after a delay to make sure it is rendered for at least
        // a few frames, otherwise its completion won't be visually noticeable.
        mHideProgressBarHandler = new Handler();
        mHideProgressBarHandler.postDelayed(
                () -> {
                    mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
                    mHideProgressBarHandler = null;
                },
                HIDE_PROGRESS_BAR_DELAY_MS);
    }

    @Override
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            int errorCode,
            GURL failingUrl,
            @LifecycleState int frameLifecycleState) {
        if (frameLifecycleState != LifecycleState.ACTIVE) return;
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
    }

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        if (!navigation.hasCommitted()) return;
        mModel.set(PaymentHandlerToolbarProperties.URL, mWebContentsRef.getVisibleUrl());
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, false);
    }

    @Override
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
        if (navigation.isSameDocument()) return;
        setSecurityState(ConnectionSecurityLevel.NONE);
    }

    @Override
    public void titleWasSet(String title) {
        mModel.set(PaymentHandlerToolbarProperties.TITLE, title);
    }

    @Override
    public void loadProgressChanged(float progress) {
        assert progress <= 1.0;
        if (progress == 1.0) return;
        // If the load restarts when the progress bar is waiting to hide, cancel the handler
        // callbacks.
        if (mHideProgressBarHandler != null) {
            mHideProgressBarHandler.removeCallbacksAndMessages(null);
            mHideProgressBarHandler = null;
        }
        mModel.set(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true);
        mModel.set(
                PaymentHandlerToolbarProperties.LOAD_PROGRESS,
                Math.max(progress, MINIMUM_LOAD_PROGRESS));
    }

    private void setSecurityState(@ConnectionSecurityLevel int securityLevel) {
        @DrawableRes int iconRes = mDelegate.getSecurityIconResource(securityLevel);
        mModel.set(PaymentHandlerToolbarProperties.SECURITY_ICON, iconRes);
        String contentDescription = mDelegate.getSecurityIconContentDescription(securityLevel);
        mModel.set(
                PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION,
                contentDescription);
    }

    @Override
    public void didChangeVisibleSecurityState() {
        setSecurityState(mDelegate.getSecurityLevel());
    }
}
