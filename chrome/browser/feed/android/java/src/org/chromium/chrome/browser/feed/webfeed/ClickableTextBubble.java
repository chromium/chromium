// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.feed.R;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.RectProvider;

/**
 * UI component that handles showing a clickable text callout bubble.
 *
 * <p>This has special styling specific to clickable text bubbles:
 *
 * <ul>
 *   <li>No arrow
 *   <li>Rounder corners
 *   <li>Smaller padding
 *   <li>Optional loading UI
 * </ul>
 *
 * <p>A loading UI using {@link LoadingView} can be shown using {@link #showLoadingUI(int)}. This
 * should be used if there is a possibility of a response time >500ms, after which the loading view
 * will show. To hide the LoadingView and dismiss the bubble, call {@link
 * #hideLoadingUI(LoadingView.Observer)}, which takes in a {@link LoadingView.Observer}, for when
 * further actions should be taken after the UI is hidden (such as showing another UI element).
 * Example below:
 *
 * <pre>{@code
 * ClickableTextBubble clickableTextBubble;
 * OnTouchListener onTouchListener = (view, motionEvent) -> {
 *     performPotentiallyLongRequest();
 *     clickableTextBubble.showLoadingUI(loadingViewContentDescriptionId);
 * };
 *
 * void potentiallyLongRequestFinished() {
 *     clickableTextBubble.hideLoadingUI(new LoadingView.Observer() {
 *         public void onHideLoadingUIComplete() {
 *             // show another UI element (eg. bubble, snackbar)
 *         }
 *     }
 * }
 *
 * }</pre>
 */
public class ClickableTextBubble extends TextBubble {
    private final Context mContext;
    private final LoadingView mLoadingView;

    /**
     * Constructs a {@link ClickableTextBubble} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawableId The resource id of the image to show at the start of the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     *     text and dismiss UX.
     * @param onTouchListener The callback for all touch events being dispatched to the bubble.
     */
    public ClickableTextBubble(
            Context context,
            View rootView,
            @StringRes int stringId,
            @StringRes int accessibilityStringId,
            RectProvider anchorRectProvider,
            @DrawableRes int imageDrawableId,
            boolean isAccessibilityEnabled,
            View.OnTouchListener onTouchListener,
            boolean inverseColor) {
        super(
                context,
                rootView,
                stringId,
                accessibilityStringId,
                /* showArrow= */ false,
                anchorRectProvider,
                imageDrawableId,
                /* isRoundBubble= */ true,
                /* inverseColor= */ inverseColor,
                isAccessibilityEnabled);
        mContext = context;
        setTouchInterceptor(onTouchListener);
        mLoadingView = mContentView.findViewById(R.id.loading_view);
    }

    @Override
    protected void updateTextStyle(TextView view, boolean isInverse) {
        if (isInverse) {
            view.setTextAppearance(R.style.TextAppearance_ClickableButtonInverse);
        } else {
            view.setTextAppearance(R.style.TextAppearance_ClickableButton);
        }
    }

    /**
     * Replaces image with loading spinner. Dismisses the entire button when loading spinner is
     * hidden.
     *
     * @param loadingViewContentDescriptionId ID of the ContentDescription for the loading spinner.
     */
    public void showLoadingUI(@StringRes int loadingViewContentDescriptionId) {
        mLoadingView.addObserver(
                new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {
                        View loadingViewContainer =
                                mContentView.findViewById(R.id.loading_view_container);
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
