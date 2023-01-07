// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * UI component that handles showing a callout bubble for "Back to top" prompt.
 */
public class BackToTopBubble {
    private final Context mContext;
    private final Runnable mClickRunnable;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final AnchoredPopupWindow mPopupWindow;

    /** The content view shown in the popup window. */
    private View mContentView;

    /**
     * Constructs a {@link BackToTopBubble} instance.
     * @param activity The activity.
     * @param context  The context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     */
    @SuppressLint("ClickableViewAccessibility")
    public BackToTopBubble(
            Activity activity, Context context, View rootView, Runnable clickRunnable) {
        mContext = context;
        mClickRunnable = clickRunnable;

        mContentView = createContentView();
        // On some versions of Android, the LayoutParams aren't set until after the popup window
        // is shown. Explicitly set the LayoutParams to avoid crashing. See
        // https://crbug.com/713759.
        mContentView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

        // Find the toolbar and use it to position the bubble below it.
        ViewGroup contentContainer = activity.findViewById(android.R.id.content);
        View toolbarView = contentContainer.findViewById(R.id.toolbar_container);
        int[] windowCoordinates = new int[2];
        toolbarView.getLocationInWindow(windowCoordinates);
        Rect rect = new Rect();
        rect.left = windowCoordinates[0];
        rect.top = windowCoordinates[1];
        rect.right = rect.left + toolbarView.getWidth();
        rect.bottom = rect.top + toolbarView.getHeight()
                + context.getResources().getDimensionPixelSize(
                        R.dimen.back_to_top_bubble_offset_below_toolbar);

        mPopupWindow = new AnchoredPopupWindow(context, rootView,
                AppCompatResources.getDrawable(mContext, R.drawable.rounded_corners), mContentView,
                new RectProvider(rect));
        mPopupWindow.setMargin(
                context.getResources().getDimensionPixelSize(R.dimen.text_bubble_margin));
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.CENTER);

        mPopupWindow.setAnimationStyle(R.style.TextBubbleAnimation);
        mPopupWindow.setTouchInterceptor((View v, MotionEvent event) -> {
            mClickRunnable.run();
            return true;
        });
    }

    /**
     * Shows the bubble. Will have no effect if the bubble is already showing.
     */
    public void show() {
        if (mPopupWindow.isShowing()) return;
        mPopupWindow.show();
    }

    /**
     * Disposes of the bubble. Will have no effect if the bubble isn't showing.
     */
    public void dismiss() {
        mPopupWindow.dismiss();
    }

    /**
     * @return Whether the bubble is currently showing.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    public View getContentView() {
        return mContentView;
    }

    private View createContentView() {
        View view = LayoutInflater.from(mContext).inflate(R.layout.back_to_top_bubble, null);
        ImageView imageView = view.findViewById(R.id.image);
        imageView.setImageDrawable(
                AppCompatResources.getDrawable(mContext, R.drawable.back_to_top_arrow));
        TextView textView = view.findViewById(R.id.message);
        textView.setText(mContext.getString(R.string.feed_back_to_top_prompt));
        return view;
    }
}
