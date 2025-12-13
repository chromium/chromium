// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.content.res.Resources;
import android.view.View;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.components.messages.MessageContainer;
import org.chromium.ui.base.ViewUtils;

/**
 * Coordinator of {@link MessageContainer}, which can adjust margins of the message container and
 * control the visibility of browser control when message is being shown.
 */
@NullMarked
public class MessageContainerCoordinator implements BrowserControlsStateProvider.Observer {
    private @Nullable MessageContainer mContainer;
    private BrowserControlsManager mControlsManager;

    /** The list of observers for the message container. */
    private final ObserverList<MessageContainerObserver> mObservers = new ObserverList<>();

    public MessageContainerCoordinator(
            MessageContainer container, BrowserControlsManager controlsManager) {
        mContainer = container;
        mControlsManager = controlsManager;
        mControlsManager.addObserver(this);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        mControlsManager.removeObserver(this);
        mContainer = null;
        mControlsManager = null;
        mObservers.clear();
    }

    public void onAnimationStart() {
        if (mContainer == null) return;
        ViewUtils.setAncestorsShouldClipChildren(mContainer, false);
    }

    public void onAnimationEnd() {
        if (mContainer == null) return;
        ViewUtils.setAncestorsShouldClipChildren(mContainer, true);
    }

    private void updateMargins() {
        assert mContainer != null;
        if (mContainer.getVisibility() != View.VISIBLE) {
            return;
        }
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mContainer.getLayoutParams();
        params.topMargin = getContainerTopOffset();
        mContainer.setLayoutParams(params);
    }

    protected void showMessageContainer() {
        assert mContainer != null;
        mContainer.setVisibility(View.VISIBLE);
        updateMargins();
        for (MessageContainerObserver o : mObservers) o.onShowMessageContainer();
    }

    protected void hideMessageContainer() {
        assert mContainer != null;
        mContainer.setVisibility(View.GONE);
        for (MessageContainerObserver o : mObservers) o.onHideMessageContainer();
    }

    /**
     * The {@link MessageContainer} view should be laid out for this method to return a meaningful
     * value.
     *
     * @return The maximum translation Y value the message banner can have as a result of the
     *     gestures. Positive values mean the message banner can be translated upward from the top
     *     of the MessagesContainer.
     */
    public int getMessageMaxTranslation() {
        assert mContainer != null;
        // The max translation is message height + controls height (adjusted for
        // Message container offsets)
        return mContainer.getMessageBannerHeight() + getContainerTopOffset();
    }

    /**
     * @return The available offset between message's top side and app's top edge.
     */
    public int getMessageTopOffset() {
        // The top offset is controls height (adjusted for Message container offsets)
        return getContainerTopOffset();
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    /**
     * Adds an observer.
     * @param observer The observer to add.
     */
    public void addObserver(MessageContainerObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer.
     * @param observer The observer to remove.
     */
    public void removeObserver(MessageContainerObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Offset of the message container from the top of the screen.
     */
    private int getContainerTopOffset() {
        assert mContainer != null;

        if (mControlsManager.getContentOffset() == 0) return 0;
        final Resources res = mContainer.getResources();
        return mControlsManager.getContentOffset()
                - res.getDimensionPixelOffset(R.dimen.message_bubble_inset);
    }
}
