// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.view.InflateException;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RemoteViews;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.util.List;

/**
 * Delegate that manages bottom bar area inside of {@link CustomTabActivity}.
 */
class CustomTabBottomBarDelegate implements FullscreenListener {
    private static final String TAG = "CustomTab";
    private static final CachedMetrics.ActionEvent REMOTE_VIEWS_SHOWN =
            new CachedMetrics.ActionEvent("CustomTabsRemoteViewsShown");
    private static final CachedMetrics.ActionEvent REMOTE_VIEWS_UPDATED =
            new CachedMetrics.ActionEvent("CustomTabsRemoteViewsUpdated");
    private static final int SLIDE_ANIMATION_DURATION_MS = 400;
    private ChromeActivity mActivity;
    private ChromeFullscreenManager mFullscreenManager;
    private ViewGroup mBottomBarView;
    @Nullable private View mBottomBarContentView;
    private CustomTabIntentDataProvider mDataProvider;
    private PendingIntent mClickPendingIntent;
    private int[] mClickableIDs;
    private boolean mShowShadow = true;

    /**
     * The override height in pixels. A value of -1 is interpreted as "not set" and means it should
     * not be used.
     */
    private int mBottomBarHeightOverride = -1;

    private OnClickListener mBottomBarClickListener = new OnClickListener() {
        @Override
        public void onClick(View v) {
            if (mClickPendingIntent == null) return;
            Intent extraIntent = new Intent();
            extraIntent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_CLICKED_ID, v.getId());
            sendPendingIntentWithUrl(mClickPendingIntent, extraIntent, mActivity);
        }
    };

    public CustomTabBottomBarDelegate(ChromeActivity activity,
            CustomTabIntentDataProvider dataProvider, ChromeFullscreenManager fullscreenManager) {
        mActivity = activity;
        mDataProvider = dataProvider;
        mFullscreenManager = fullscreenManager;
        fullscreenManager.addListener(this);
    }

    /**
     * Makes the bottom bar area to show, if any.
     */
    public void showBottomBarIfNecessary() {
        if (!shouldShowBottomBar()) return;

        getBottomBarView()
                .findViewById(R.id.bottombar_shadow)
                .setVisibility(mShowShadow ? View.VISIBLE : View.GONE);

        if (mBottomBarContentView != null) {
            getBottomBarView().addView(mBottomBarContentView);
            mBottomBarContentView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    mBottomBarContentView.removeOnLayoutChangeListener(this);
                    mFullscreenManager.setBottomControlsHeight(getBottomBarHeight());
                }
            });
            return;
        }

        RemoteViews remoteViews = mDataProvider.getBottomBarRemoteViews();
        if (remoteViews != null) {
            REMOTE_VIEWS_SHOWN.record();
            mClickableIDs = mDataProvider.getClickableViewIDs();
            mClickPendingIntent = mDataProvider.getRemoteViewsPendingIntent();
            showRemoteViews(remoteViews);
            return;
        }

        List<CustomButtonParams> items = mDataProvider.getCustomButtonsOnBottombar();
        if (items.isEmpty()) return;
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setId(R.id.custom_tab_bottom_bar_wrapper);
        layout.setBackgroundColor(mDataProvider.getBottomBarColor());
        for (CustomButtonParams params : items) {
            if (params.showOnToolbar()) continue;
            final PendingIntent pendingIntent = params.getPendingIntent();
            OnClickListener clickListener = null;
            if (pendingIntent != null) {
                clickListener = v -> sendPendingIntentWithUrl(pendingIntent, null, mActivity);
            }
            layout.addView(
                    params.buildBottomBarButton(mActivity, getBottomBarView(), clickListener));
        }
        getBottomBarView().addView(layout);
    }

    /**
     * Updates the custom buttons on bottom bar area.
     * @param params The {@link CustomButtonParams} that describes the button to update.
     */
    public void updateBottomBarButtons(CustomButtonParams params) {
        ImageButton button = (ImageButton) getBottomBarView().findViewById(params.getId());
        button.setContentDescription(params.getDescription());
        button.setImageDrawable(params.getIcon(mActivity));
    }

    /**
     * Updates the RemoteViews on the bottom bar. If the given remote view is null, animates the
     * bottom bar out.
     * @param remoteViews The new remote view hierarchy sent from the client.
     * @param clickableIDs Array of view ids, the onclick event of which is intercepcted by chrome.
     * @param pendingIntent The {@link PendingIntent} that will be sent on clicking event.
     * @return Whether the update is successful.
     */
    public boolean updateRemoteViews(RemoteViews remoteViews, int[] clickableIDs,
            PendingIntent pendingIntent) {
        REMOTE_VIEWS_UPDATED.record();
        if (remoteViews == null) {
            if (mBottomBarView == null) return false;
            hideBottomBar();
            mClickableIDs = null;
            mClickPendingIntent = null;
            return true;
        } else {
            // TODO: investigate updating the RemoteViews without replacing the entire hierarchy.
            mClickableIDs = clickableIDs;
            mClickPendingIntent = pendingIntent;
            if (getBottomBarView().getChildCount() > 1) getBottomBarView().removeViewAt(1);
            return showRemoteViews(remoteViews);
        }
    }

    /**
     * Sets the content of the bottom bar.
     */
    public void setBottomBarContentView(View view) {
        mBottomBarContentView = view;
    }

    /**
     * Sets the visibility of the bottom bar shadow.
     */
    public void setShowShadow(boolean show) {
        mShowShadow = show;
    }

    /**
     * @return The height of the bottom bar, excluding its top shadow.
     */
    public int getBottomBarHeight() {
        if (!shouldShowBottomBar() || mBottomBarView == null
                || mBottomBarView.getChildCount() < 2) {
            return 0;
        }
        if (mBottomBarHeightOverride != -1) return mBottomBarHeightOverride;
        return mBottomBarView.getChildAt(1).getHeight();
    }

    /**
     * Sets a height override for the bottom bar. If this value is not set, the height of the
     * content is used instead.
     *
     * @param height The override height in pixels. A value of -1 is interpreted as "not set" and
     *     means it will not be used.
     */
    public void setBottomBarHeight(int height) {
        mBottomBarHeightOverride = height;
    }

    /**
     * Gets the {@link ViewGroup} of the bottom bar. If it has not been inflated, inflate it first.
     */
    private ViewGroup getBottomBarView() {
        if (mBottomBarView == null) {
            assert isViewReady() : "The required view stub couldn't be found! (Called too early?)";
            ViewStub bottomBarStub = ((ViewStub) mActivity.findViewById(R.id.bottombar_stub));
            mBottomBarView = (ViewGroup) bottomBarStub.inflate();
        }
        return mBottomBarView;
    }

    public void addOverlayPanelManagerObserver() {
        if (mActivity.getCompositorViewHolder().getLayoutManager() == null) return;
        OverlayPanelManager manager =
                mActivity.getCompositorViewHolder().getLayoutManager().getOverlayPanelManager();

        manager.addObserver(new OverlayPanelManagerObserver() {
            @Override
            public void onOverlayPanelShown() {
                if (mBottomBarView == null) return;
                mBottomBarView.animate()
                        .alpha(0)
                        .setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE)
                        .setDuration(SLIDE_ANIMATION_DURATION_MS)
                        .withEndAction(() -> mBottomBarView.setVisibility(View.GONE))
                        .start();
            }
            @Override
            public void onOverlayPanelHidden() {
                if (mBottomBarView == null) return;
                mBottomBarView.setVisibility(View.VISIBLE);
                mBottomBarView.animate()
                        .alpha(1)
                        .setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE)
                        .setDuration(SLIDE_ANIMATION_DURATION_MS)
                        .start();
            }
        });
    }

    private void hideBottomBar() {
        if (mBottomBarView == null) return;
        mBottomBarView.animate().alpha(0f).translationY(mBottomBarView.getHeight())
                .setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE)
                .setDuration(SLIDE_ANIMATION_DURATION_MS)
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        ((ViewGroup) mBottomBarView.getParent()).removeView(mBottomBarView);
                        mBottomBarView = null;
                    }
                }).start();
        mFullscreenManager.setBottomControlsHeight(0);
    }

    private boolean showRemoteViews(RemoteViews remoteViews) {
        final View inflatedView;
        try {
            inflatedView =
                    remoteViews.apply(ContextUtils.getApplicationContext(), getBottomBarView());
        } catch (RemoteViews.ActionException | InflateException | Resources.NotFoundException e) {
            Log.e(TAG, "Failed to inflate the RemoteViews", e);
            return false;
        }

        if (mClickableIDs != null && mClickPendingIntent != null) {
            for (int id : mClickableIDs) {
                if (id < 0) return false;
                View view = inflatedView.findViewById(id);
                if (view != null) view.setOnClickListener(mBottomBarClickListener);
            }
        }
        getBottomBarView().addView(inflatedView, 1);
        inflatedView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                inflatedView.removeOnLayoutChangeListener(this);
                mFullscreenManager.setBottomControlsHeight(getBottomBarHeight());
            }
        });
        return true;
    }

    private static void sendPendingIntentWithUrl(PendingIntent pendingIntent, Intent extraIntent,
            ChromeActivity activity) {
        Intent addedIntent = extraIntent == null ? new Intent() : new Intent(extraIntent);
        Tab tab = activity.getActivityTab();
        if (tab != null) addedIntent.setData(Uri.parse(tab.getUrl()));
        try {
            pendingIntent.send(activity, 0, addedIntent, null, null);
        } catch (CanceledException e) {
            Log.e(TAG, "CanceledException when sending pending intent.");
        }
    }

    private boolean shouldShowBottomBar() {
        return mBottomBarContentView != null || mDataProvider.shouldShowBottomBar();
    }

    /**
     * Returns whether the view was or can be inflated.
     * @return True if the ViewStub is present or was inflated. False otherwise.
     */
    private boolean isViewReady() {
        return mBottomBarView != null || mActivity.findViewById(R.id.bottombar_stub) != null;
    }

    // FullscreenListener methods
    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset,
            boolean needsAnimate) {
        if (mBottomBarView != null) mBottomBarView.setTranslationY(bottomOffset);
        // If the bottom bar is not visible use the top controls as a guide to set state.
        float offset = getBottomBarHeight() == 0 ? topOffset : bottomOffset;
        float height = getBottomBarHeight() == 0 ? mFullscreenManager.getTopControlsHeight()
                                                 : mFullscreenManager.getBottomControlsHeight();
        // Avoid spamming this callback across process boundaries, by only sending messages at
        // absolute transitions.
        if (Math.abs(offset) == height || offset == 0) {
            CustomTabsConnection.getInstance().onBottomBarScrollStateChanged(
                    mDataProvider.getSession(), offset != 0);
        }
    }

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        if (!isViewReady()) return;
        getBottomBarView().setTranslationY(mFullscreenManager.getBottomControlOffset());
    }

    @Override
    public void onUpdateViewportSize() {
        if (mBottomBarView == null) return; // Check bottom bar view but don't inflate it.
        // Hide the container of the bottom bar while the extension is showing. This doesn't
        // affect the content.
        boolean keyboardExtensionHidesBottomBar = mActivity.getManualFillingController()
                                                          .getKeyboardExtensionSizeManager()
                                                          .getKeyboardExtensionHeight()
                > 0;
        if (keyboardExtensionHidesBottomBar) {
            getBottomBarView().setVisibility(View.GONE);
            mFullscreenManager.setBottomControlsHeight(0);
        } else {
            getBottomBarView().setVisibility(View.VISIBLE);
            mFullscreenManager.setBottomControlsHeight(getBottomBarHeight());
        }
    }

    @Override
    public void onContentOffsetChanged(float offset) { }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) { }
}
