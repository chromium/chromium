// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;

import java.util.HashSet;

/**
 * The coordinator for a status indicator that is positioned below the status bar and is persistent.
 * Typically used to relay status, e.g. indicate user is offline.
 */
public class StatusIndicatorCoordinator {
    /** An observer that will be notified of the changes to the status indicator, e.g. height. */
    public interface StatusIndicatorObserver {
        /**
         * Called when the height of the status indicator changes.
         * @param newHeight The new height in pixels.
         */
        void onStatusIndicatorHeightChanged(int newHeight);
    }

    private PropertyModel mModel;
    private View mView;
    private StatusIndicatorSceneLayer mSceneLayer;
    private HashSet<StatusIndicatorObserver> mObservers = new HashSet<>();

    public StatusIndicatorCoordinator(Activity activity, ResourceManager resourceManager) {
        // TODO(crbug.com/1005843): Create this view lazily if/when we need it. This is a task for
        // when we have the public API figured out.
        final ViewStub stub = activity.findViewById(R.id.status_indicator_stub);
        ViewResourceFrameLayout root = (ViewResourceFrameLayout) stub.inflate();
        mView = root;
        mSceneLayer = new StatusIndicatorSceneLayer(root);
        mModel = new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                         .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, false)
                         .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                         .build();
        PropertyModelChangeProcessor.create(mModel,
                new StatusIndicatorViewBinder.ViewHolder(root, mSceneLayer),
                StatusIndicatorViewBinder::bind);
        resourceManager.getDynamicResourceLoader().registerResource(
                root.getId(), root.getResourceAdapter());
    }

    /**
     * Set the {@link String} the status indicator should display.
     * @param statusText The string.
     */
    public void setStatusText(String statusText) {
        mModel.set(StatusIndicatorProperties.STATUS_TEXT, statusText);
    }

    /**
     * Set the {@link Drawable} the status indicator should display next to the status text.
     * @param statusIcon The icon drawable.
     */
    public void setStatusIcon(Drawable statusIcon) {
        mModel.set(StatusIndicatorProperties.STATUS_ICON, statusIcon);
    }

    // TODO(sinansahin): With animation.
    // TODO(sinansahin): Destroy the view when not needed.
    /** Show the status indicator. */
    public void show() {
        mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, true);
        mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, true);
        // TODO(crbug.com/1005843): We will need a measure pass before we can get the real height of
        // this view. We should keep this in mind when inflating the view lazily.
        mView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                final int height = v.getHeight();
                for (StatusIndicatorObserver observer : mObservers) {
                    observer.onStatusIndicatorHeightChanged(height);
                }
                mView.removeOnLayoutChangeListener(this);
            }
        });
    }

    // TODO(sinansahin): With animation as well.
    /** Hide the status indicator. */
    public void hide() {
        mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false);
        mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE, false);

        for (StatusIndicatorObserver observer : mObservers) {
            observer.onStatusIndicatorHeightChanged(0);
        }
    }

    public void addObserver(StatusIndicatorObserver observer) {
        mObservers.add(observer);
    }

    public void removeObserver(StatusIndicatorObserver observer) {
        mObservers.remove(observer);
    }

    /**
     * Is the status indicator currently visible.
     * @return True if visible.
     */
    public boolean isVisible() {
        return mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE);
    }

    /**
     * @return The {@link StatusIndicatorSceneLayer}.
     */
    public StatusIndicatorSceneLayer getSceneLayer() {
        return mSceneLayer;
    }
}
