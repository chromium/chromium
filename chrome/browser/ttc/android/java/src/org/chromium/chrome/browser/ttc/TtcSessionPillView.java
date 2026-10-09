// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;
import org.chromium.ui.widget.RectProvider;

/**
 * The floating session pill. It lives in an {@link AnchoredPopupWindow} pinned to the bottom-end
 * corner of the content area instead of a view in the activity layout, so it needs nothing from
 * main.xml. Constructed lazily, the first time a session becomes visible.
 */
@NullMarked
class TtcSessionPillView {
    // How much the microphone icon grows at full input level.
    private static final float MAX_ICON_SCALE_DELTA = 0.4f;

    private final AnchoredPopupWindow mPopup;
    private final View mContent;
    private final TextView mStatus;
    private final ImageView mIcon;

    /**
     * @param anchorView The view whose bottom-end corner the pill is pinned to, normally the
     *     activity's content area.
     */
    TtcSessionPillView(View anchorView) {
        Context context = anchorView.getContext();
        Resources res = context.getResources();

        mContent = LayoutInflater.from(context).inflate(R.layout.ttc_session_pill, null);
        mStatus = mContent.findViewById(R.id.ttc_session_pill_status);
        mIcon = mContent.findViewById(R.id.ttc_session_pill_icon);
        // TalkBack reads the status text as the pill's label; this names what a tap does.
        ViewCompat.replaceAccessibilityAction(
                mContent,
                AccessibilityActionCompat.ACTION_CLICK,
                res.getString(R.string.ttc_session_end_description),
                null);

        Drawable background =
                assumeNonNull(context.getDrawable(R.drawable.ttc_session_pill_background));
        RectProvider anchor =
                new BottomEndCornerRectProvider(
                        anchorView,
                        res.getDimensionPixelSize(R.dimen.ttc_session_pill_margin_horizontal),
                        res.getDimensionPixelSize(R.dimen.ttc_session_pill_margin_bottom));
        mPopup =
                // The popup drops its content view on dismiss and asks the builder again on the
                // next show; returning the same view keeps the cached children valid.
                new AnchoredPopupWindow.Builder(
                                context, anchorView, background, unused -> mContent, anchor)
                        .setPreferredVerticalOrientation(VerticalOrientation.ABOVE)
                        .setPreferredHorizontalOrientation(
                                HorizontalOrientation.MAX_AVAILABLE_SPACE)
                        .setFocusable(false)
                        .setTouchModal(false)
                        .setOutsideTouchable(false)
                        .setDismissOnTouchInteraction(false)
                        // The pill is exactly one touch target tall, which is under the popup's
                        // default minimum (it adds a 1dp margin on each side).
                        .setAllowNonTouchableSize(true)
                        .setElevation(res.getDimension(R.dimen.ttc_session_pill_elevation))
                        .build();
    }

    void setVisible(boolean visible) {
        if (visible) {
            mPopup.show();
        } else {
            mPopup.dismiss();
        }
    }

    void setStatusText(@StringRes int resId) {
        mStatus.setText(resId);
        // The popup window is sized explicitly, so ask it to re-measure for the new text.
        if (mPopup.isShowing()) mPopup.onRectChanged();
    }

    void setAudioLevel(float level) {
        float scale = 1f + MAX_ICON_SCALE_DELTA * level;
        mIcon.setScaleX(scale);
        mIcon.setScaleY(scale);
    }

    void setOnClickListener(View.@Nullable OnClickListener listener) {
        mContent.setOnClickListener(listener);
    }

    void destroy() {
        mPopup.dismiss();
    }

    /**
     * Provides a zero-size anchor at a view's bottom-end corner, inset by the pill margins. With
     * {@link HorizontalOrientation#MAX_AVAILABLE_SPACE} and {@link VerticalOrientation#ABOVE} the
     * popup then opens inward from that corner in both LTR and RTL layouts.
     */
    private static class BottomEndCornerRectProvider extends RectProvider
            implements View.OnLayoutChangeListener {
        private final View mView;
        private final int mMarginHorizontal;
        private final int mMarginBottom;
        private final int[] mLocation = new int[2];

        BottomEndCornerRectProvider(View view, int marginHorizontal, int marginBottom) {
            mView = view;
            mMarginHorizontal = marginHorizontal;
            mMarginBottom = marginBottom;
            update();
        }

        @Override
        public void startObserving(Observer observer) {
            super.startObserving(observer);
            mView.addOnLayoutChangeListener(this);
            update();
        }

        @Override
        public void stopObserving(Observer observer) {
            super.stopObserving(observer);
            mView.removeOnLayoutChangeListener(this);
        }

        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            update();
        }

        private void update() {
            mView.getLocationInWindow(mLocation);
            boolean rtl = mView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            int x =
                    rtl
                            ? mLocation[0] + mMarginHorizontal
                            : mLocation[0] + mView.getWidth() - mMarginHorizontal;
            int y = mLocation[1] + mView.getHeight() - mMarginBottom;
            setRect(new Rect(x, y, x, y));
        }
    }
}
