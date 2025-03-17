// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;

/** Manages the tooltip that is shown when a CompositorButton is hovered. */
public class TooltipManager {
    private final TooltipEventHandler mHandler = new TooltipEventHandler();
    private CompositorButton mCurrentButton;
    private final FrameLayout mTooltipView;
    private final Supplier<Float> mTopPaddingSupplier; // in dp units

    private static final int MESSAGE_SHOW_TOOLTIP = 1;
    private static final int TOOLTIP_DELAY_MS = 1000;

    public TooltipManager(FrameLayout tooltipView, Supplier<Float> topPaddingSupplierDp) {
        mTooltipView = tooltipView;
        mTopPaddingSupplier = topPaddingSupplierDp;
    }

    public void setHovered(CompositorButton button, boolean isHovered) {
        if (isHovered && mCurrentButton != button) {
            hideImmediately();
            showWithDelayFor(button);
        } else if (!isHovered && mCurrentButton == button) {
            hideImmediately();
        }
    }

    private void clearPendingMessages() {
        mHandler.removeMessages(MESSAGE_SHOW_TOOLTIP);
    }

    private void showWithDelayFor(CompositorButton targetButton) {
        mCurrentButton = targetButton;
        clearPendingMessages();

        Message message = mHandler.obtainMessage(MESSAGE_SHOW_TOOLTIP, targetButton);
        mHandler.sendMessageDelayed(message, TOOLTIP_DELAY_MS);
    }

    void showImmediatelyFor(CompositorButton targetButton) {
        mCurrentButton = targetButton;

        ((TextView) mTooltipView.findViewById(R.id.tooltip_label))
                .setText(targetButton.getAccessibilityDescription());

        mTooltipView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        float tooltipWidthPx = mTooltipView.getMeasuredWidth();
        mTooltipView.setX(calculateTooltipXPx(targetButton, tooltipWidthPx));
        mTooltipView.setY(calculateTooltipYPx(targetButton));

        mTooltipView.setVisibility(View.VISIBLE);
    }

    void hideImmediately() {
        mCurrentButton = null;
        clearPendingMessages();
        mTooltipView.setVisibility(View.GONE);
    }

    private float calculateTooltipXPx(CompositorButton targetButton, float tooltipWidthPx) {
        float idealXCenterDp = targetButton.getDrawX() + targetButton.getWidth() / 2f;
        float idealXCenterPx = idealXCenterDp * getDisplayDensity();
        float idealXLeftPx = idealXCenterPx - tooltipWidthPx / 2f;
        float safeXLeftPx = MathUtils.clamp(idealXLeftPx, 0, getWindowWidthPx() - tooltipWidthPx);
        return safeXLeftPx;
    }

    private float calculateTooltipYPx(CompositorButton targetButton) {
        float yDp = mTopPaddingSupplier.get() + targetButton.getDrawY() + targetButton.getHeight();
        float yPx = yDp * getDisplayDensity();
        return yPx;
    }

    private float getWindowWidthPx() {
        return mTooltipView.getContext().getResources().getDisplayMetrics().widthPixels;
    }

    private float getDisplayDensity() {
        return mTooltipView.getContext().getResources().getDisplayMetrics().density;
    }

    protected FrameLayout getTooltipViewForTesting() {
        return mTooltipView;
    }

    @SuppressLint("HandlerLeak")
    private class TooltipEventHandler extends Handler {
        @Override
        public void handleMessage(Message m) {
            if (m.what != MESSAGE_SHOW_TOOLTIP) {
                assert false : "TooltipEventHandler got unknown message " + m.what;
            }
            showImmediatelyFor((CompositorButton) m.obj);
        }
    }
}
