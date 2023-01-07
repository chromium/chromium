// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.browser.feed.R;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.RectProvider;

/**
 * Provides a text bubble with a dark shadow to support a medium dark color.  We need a darker
 * shadow than other images such as navigation_bubble_shadow provide to prevent the optical illusion
 * of the shadow being a part of the bitmap.  However, if we add the images for the new shadow to
 * ClickableTextBubble, they also get used by WebLayer, which doesn't need this shadow.  To prevent
 * increasing the size of the chrome APK, we include the bitmaps and the class to use them here with
 * the other feed code.
 *
 * UI component that handles showing a clickable text callout bubble.
 *
 * <p>This has special styling specific to clickable text bubbles:
 * <ul>
 *     <li>No arrow
 *     <li>Rounder corners
 *     <li>Smaller padding
 *     <li>Shadow
 *     <li>Optional loading UI
 * </ul>
 *
 * <p>A loading UI using {@link LoadingView} can be shown using {@link #showLoadingUI(int)}. This
 * should be used if there is a possibility of a response time >500ms, after which the loading
 * view will show. To hide the LoadingView and dismiss the bubble, call
 * {@link #hideLoadingUI(LoadingView.Observer)}, which takes in a {@link LoadingView.Observer},
 * for when further actions should be taken after the UI is hidden (such as showing another UI
 * element). Example below:
 *
 *  <pre>{@code
 *      ShadowedClickableTextBubble clickableTextBubble;
 *      OnTouchListener onTouchListener = (view, motionEvent) -> {
 *          performPotentiallyLongRequest();
 *          clickableTextBubble.showLoadingUI(loadingViewContentDescriptionId);
 *      };
 *
 *      void potentiallyLongRequestFinished() {
 *          clickableTextBubble.hideLoadingUI(new LoadingView.Observer() {
 *              public void onHideLoadingUIComplete() {
 *                  // show another UI element (eg. bubble, snackbar)
 *              }
 *          }
 *      }
 *  }</pre>
 */
public class ShadowedClickableTextBubble extends TextBubble {
    private final Context mContext;
    private final LoadingView mLoadingView;
    private final boolean mInverseColor;

    /**
     * Constructs a {@link ShadowedClickableTextBubble} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawableId The resource id of the image to show at the start of the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     * text and dismiss UX.
     * @param onTouchListener The callback for all touch events being dispatched to the bubble.
     */
    public ShadowedClickableTextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, RectProvider anchorRectProvider,
            @DrawableRes int imageDrawableId, boolean isAccessibilityEnabled,
            View.OnTouchListener onTouchListener, boolean inverseColor) {
        super(context, rootView, stringId, accessibilityStringId, /*showArrow=*/false,
                anchorRectProvider, imageDrawableId, /*isRoundBubble=*/true,
                /*inverseColor=*/inverseColor, isAccessibilityEnabled);
        mContext = context;
        mInverseColor = inverseColor;
        setTouchInterceptor(onTouchListener);
        mLoadingView = mContentView.findViewById(R.id.loading_view);
    }

    /** Get the backgound to use. We use a color button with a dark shadow. */
    @Override
    public Drawable getBackground(Context context, boolean showArrow, boolean isRoundBubble) {
        Drawable background = ResourcesCompat.getDrawable(
                context.getResources(), R.drawable.follow_accelerator_shadow, null);
        return background;
    }

    /**
     * Replaces image with loading spinner. Dismisses the entire button when loading spinner is
     * hidden.
     *
     * @param loadingViewContentDescriptionId ID of the ContentDescription for the loading spinner.
     */
    public void showLoadingUI(@StringRes int loadingViewContentDescriptionId) {
        mLoadingView.addObserver(new LoadingView.Observer() {
            @Override
            public void onShowLoadingUIComplete() {
                View loadingViewContainer = mContentView.findViewById(R.id.loading_view_container);
                loadingViewContainer.setVisibility(View.VISIBLE);
                loadingViewContainer.setContentDescription(
                        mContext.getString(loadingViewContentDescriptionId));
                mContentView.findViewById(R.id.image).setVisibility(View.GONE);
                setAutoDismissTimeout(NO_TIMEOUT);
            }

            @Override
            public void onHideLoadingUIComplete() {
                dismiss();
            }
        });
        mLoadingView.showLoadingUI();
    }

    /**
     * Exposes {@link LoadingView#hideLoadingUI()} and adds a {@link LoadingView.Observer} to the
     * {@link LoadingView}.
     *
     * @param loadingViewObserver Observer to add to the {@link LoadingView}.
     */
    public void hideLoadingUI(LoadingView.Observer loadingViewObserver) {
        mLoadingView.addObserver(loadingViewObserver);
        mLoadingView.hideLoadingUI();
    }

    public void destroy() {
        mLoadingView.destroy();
    }
}
