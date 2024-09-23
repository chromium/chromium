// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.net.Uri;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RemoteViews;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.night_mode.RemoteViewsWithNightModeInflater;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.ui.base.ViewportInsets;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;

import java.util.List;

import javax.inject.Inject;

/** Delegate that manages bottom bar area inside of {@link CustomTabActivity}. */
@ActivityScope
public class CustomTabBottomBarDelegate
        implements BrowserControlsStateProvider.Observer, SwipeGestureListener.SwipeHandler {
    private static final String TAG = "CustomTab";
    private static final int SLIDE_ANIMATION_DURATION_MS = 400;

    /**
     * Provides an interface for updating custom button states based on provided parameters.
     *
     * <p>Implementations of this interface should define the logic for determining how a custom
     * button's appearance or behavior should change in response to the given parameters.
     */
    public interface CustomButtonsUpdater {

        /**
         * Updates the state of a bottom bar button based on the provided parameters.
         *
         * @param params The parameters containing information relevant to the button update.
         * @return {@code true} if the button was successfully updated, {@code false} otherwise.
         */
        boolean updateBottomBarButton(CustomButtonParams params);
    }

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final BrowserServicesIntentDataProvider mDataProvider;
    private final Supplier<Tab> mTabProvider;
    private final CustomTabNightModeStateController mNightModeStateController;
    private final SystemNightModeMonitor mSystemNightModeMonitor;

    private CustomTabBottomBarView mBottomBarView;
    @Nullable private View mBottomBarContentView;
    @Nullable private CustomButtonsUpdater mCustomButtonsUpdater;
    private PendingIntent mClickPendingIntent;
    private int[] mClickableIDs;
    private boolean mShowShadow = true;
    private @Nullable PendingIntent mSwipeUpPendingIntent;
    private boolean mKeepContentView;

    /**
     * The override height in pixels. A value of -1 is interpreted as "not set" and means it should
     * not be used.
     */
    private int mBottomBarHeightOverride = -1;

    private OnClickListener mBottomBarClickListener =
            new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mClickPendingIntent == null) return;
                    Intent extraIntent = new Intent();
                    int originalId = (Integer) v.getTag(R.id.view_id_tag_key);
                    extraIntent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_CLICKED_ID, originalId);
                    sendPendingIntentWithUrl(
                            mClickPendingIntent, extraIntent, mActivity, mTabProvider);
                }
            };

    @Inject
    public CustomTabBottomBarDelegate(
            Activity activity,
            WindowAndroid windowAndroid,
            BrowserServicesIntentDataProvider dataProvider,
            BrowserControlsSizer browserControlsSizer,
            CustomTabNightModeStateController nightModeStateController,
            SystemNightModeMonitor systemNightModeMonitor,
            CustomTabActivityTabProvider tabProvider,
            CustomTabCompositorContentInitializer compositorContentInitializer) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mDataProvider = dataProvider;
        mBrowserControlsSizer = browserControlsSizer;
        mNightModeStateController = nightModeStateController;
        mSystemNightModeMonitor = systemNightModeMonitor;
        mTabProvider = () -> tabProvider.getTab();
        browserControlsSizer.addObserver(this);
        mKeepContentView = false;
        compositorContentInitializer.addCallback(this::addOverlayPanelManagerObserver);

        Callback<ViewportInsets> insetObserver = this::onViewportInsetChange;
        // TODO(REVIEW): Is it ok this doesn't remove itself?
        mWindowAndroid.getApplicationBottomInsetSupplier().addObserver(insetObserver);
    }

    /** Makes the bottom bar area to show, if any. */
    public void showBottomBarIfNecessary() {
        if (!shouldShowBottomBar()) return;

        getBottomBarView()
                .findViewById(R.id.bottombar_shadow)
                .setVisibility(mShowShadow ? View.VISIBLE : View.GONE);

        if (mDataProvider.getSecondaryToolbarSwipeUpPendingIntent() != null) {
            startListeningForSwipeUpGestures(
                    mDataProvider.getSecondaryToolbarSwipeUpPendingIntent());
        }

        if (mBottomBarContentView != null) {
            getBottomBarView().addView(mBottomBarContentView);
            mBottomBarContentView.addOnLayoutChangeListener(
                    new OnLayoutChangeListener() {
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
                            mBottomBarContentView.removeOnLayoutChangeListener(this);
                            setBottomControlsHeight(getBottomBarHeight());
                        }
                    });
            return;
        }

        RemoteViews remoteViews = mDataProvider.getBottomBarRemoteViews();
        if (remoteViews != null) {
            RecordUserAction.record("CustomTabsRemoteViewsShown");
            mClickableIDs = mDataProvider.getClickableViewIDs();
            mClickPendingIntent = mDataProvider.getRemoteViewsPendingIntent();
            showRemoteViews(remoteViews);
            return;
        }

        List<CustomButtonParams> items = mDataProvider.getCustomButtonsOnBottombar();
        if (items.isEmpty()) return;
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setId(R.id.custom_tab_bottom_bar_wrapper);
        layout.setBackgroundColor(mDataProvider.getColorProvider().getBottomBarColor());
        for (CustomButtonParams params : items) {
            if (params.showOnToolbar()) continue;
            final PendingIntent pendingIntent = params.getPendingIntent();
            OnClickListener clickListener = null;
            if (pendingIntent != null) {
                clickListener =
                        v -> sendPendingIntentWithUrl(pendingIntent, null, mActivity, mTabProvider);
            }
            layout.addView(
                    params.buildBottomBarButton(mActivity, getBottomBarView(), clickListener));
        }
        getBottomBarView().addView(layout);
    }

    /**
     * Updates the custom buttons on bottom bar area.
     *
     * @param params The {@link CustomButtonParams} that describes the button to update.
     */
    public void updateBottomBarButtons(CustomButtonParams params) {
        if (mCustomButtonsUpdater != null && mCustomButtonsUpdater.updateBottomBarButton(params)) {
            return;
        }
        ImageButton button = (ImageButton) getBottomBarView().findViewById(params.getId());
        button.setContentDescription(params.getDescription());
        button.setImageDrawable(params.getIcon(mActivity));
    }

    /**
     * Sets the updater responsible for managing the state of custom buttons.
     *
     * <p>If the bottom bar view is set with {@link #setBottomBarContentView} you should always
     * provide customButtonsUpdater.
     *
     * @param customButtonsUpdater The {@link CustomButtonsUpdater} implementation that will handle
     *     the logic for updating custom button states, overriding the default logic.
     */
    public void setCustomButtonsUpdater(CustomButtonsUpdater customButtonsUpdater) {
        mCustomButtonsUpdater = customButtonsUpdater;
    }

    /**
     * Updates the RemoteViews on the bottom bar. If the given remote view is null, animates the
     * bottom bar out.
     *
     * @param remoteViews The new remote view hierarchy sent from the client.
     * @param clickableIDs Array of view ids, the onclick event of which is intercepcted by chrome.
     * @param pendingIntent The {@link PendingIntent} that will be sent on clicking event.
     * @return Whether the update is successful.
     */
    public boolean updateRemoteViews(
            RemoteViews remoteViews, int[] clickableIDs, PendingIntent pendingIntent) {
        // If the contentView is already set, it should have priority to keep being displayed over
        // any remote views that are trying to be updated.
        if (mBottomBarContentView != null && mKeepContentView) {
            return false;
        }
        RecordUserAction.record("CustomTabsRemoteViewsUpdated");
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
     * Updates the {@link PendingIntent} to be sent when the user swipes up from the toolbar.
     * @param pendingIntent The {@link PendingIntent}.
     * @return Whether the update is successful.
     */
    public boolean updateSwipeUpPendingIntent(PendingIntent pendingIntent) {
        if (pendingIntent == null) {
            if (mBottomBarView == null) return false;
            stopListeningForSwipeUpGestures();
        } else {
            startListeningForSwipeUpGestures(pendingIntent);
        }
        return true;
    }

    /** Sets the content of the bottom bar. */
    public void setBottomBarContentView(View view) {
        mBottomBarContentView = view;
    }

    /** Sets the visibility of the bottom bar shadow. */
    public void setShowShadow(boolean show) {
        mShowShadow = show;
    }

    /**
     * Determines the behavior of the bottom bar content view when using RemoteViews.
     *
     * <p>By default, RemoteViews may replace the bottom bar content view. If the bottom bar view
     * set with {@link #setBottomBarContentView} should always displayed, set this value to {@code
     * true}.
     *
     * <p>**Important Note:** Enabling this feature will prevent RemoteViews from being used via
     * {@link #updateRemoteViews}.
     */
    public void setKeepContentView(boolean keep) {
        mKeepContentView = keep;
    }

    /**
     * @return The height of the bottom bar, excluding its top shadow.
     */
    public int getBottomBarHeight() {
        if (!shouldShowBottomBar()
                || mBottomBarView == null
                || mBottomBarView.getChildCount() < 2) {
            return 0;
        }
        if (mBottomBarHeightOverride != -1) return mBottomBarHeightOverride;
        return mBottomBarView.getHeight();
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
            ViewStub bottomBarStub = mActivity.findViewById(R.id.bottombar_stub);
            mBottomBarView = (CustomTabBottomBarView) bottomBarStub.inflate();
        }
        return mBottomBarView;
    }

    public void addOverlayPanelManagerObserver(LayoutManagerImpl layoutDriver) {
        layoutDriver
                .getOverlayPanelManager()
                .addObserver(
                        new OverlayPanelManagerObserver() {
                            @Override
                            public void onOverlayPanelShown() {
                                if (mBottomBarView == null) return;
                                mBottomBarView
                                        .animate()
                                        .alpha(0)
                                        .setInterpolator(
                                                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                                        .setDuration(SLIDE_ANIMATION_DURATION_MS)
                                        .withEndAction(
                                                () -> mBottomBarView.setVisibility(View.GONE))
                                        .start();
                            }

                            @Override
                            public void onOverlayPanelHidden() {
                                if (mBottomBarView == null) return;
                                mBottomBarView.setVisibility(View.VISIBLE);
                                mBottomBarView
                                        .animate()
                                        .alpha(1)
                                        .setInterpolator(
                                                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                                        .setDuration(SLIDE_ANIMATION_DURATION_MS)
                                        .start();
                            }
                        });
    }

    /**
     * This method remove bottomBarView completely.
     * If you need to hide it temporarily use {@link #hideBottomBar(boolean)}.
     */
    private void hideBottomBar() {
        if (mBottomBarView == null) return;
        stopListeningForSwipeUpGestures();
        mBottomBarView
                .animate()
                .alpha(0f)
                .translationY(mBottomBarView.getHeight())
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                .setDuration(SLIDE_ANIMATION_DURATION_MS)
                .withEndAction(
                        new Runnable() {
                            @Override
                            public void run() {
                                ((ViewGroup) mBottomBarView.getParent()).removeView(mBottomBarView);
                                mBottomBarView = null;
                            }
                        })
                .start();
        setBottomControlsHeight(0);
    }

    private void transformViewIds(View view) {
        // Store the old id in a tag. The tag key here does not matter as long
        // as it is unique across all tags.
        view.setTag(R.id.view_id_tag_key, view.getId());
        view.setId(View.NO_ID);
        if (view instanceof ViewGroup group) {
            final int childCount = group.getChildCount();
            for (int i = 0; i < childCount; i++) {
                final View child = group.getChildAt(i);
                transformViewIds(child);
            }
        }
    }

    private boolean showRemoteViews(RemoteViews remoteViews) {
        final View inflatedView =
                RemoteViewsWithNightModeInflater.inflate(
                        remoteViews,
                        getBottomBarView(),
                        mNightModeStateController.isInNightMode(),
                        mSystemNightModeMonitor.isSystemNightModeOn());

        if (inflatedView == null) return false;

        if (mClickableIDs != null && mClickPendingIntent != null) {
            for (int id : mClickableIDs) {
                if (id < 0) return false;
                View view = inflatedView.findViewById(id);
                if (view != null) view.setOnClickListener(mBottomBarClickListener);
            }
        }

        // Set all views' ids to be View.NO_ID to prevent them clashing with
        // chrome's resource ids. See http://crbug.com/1061872
        transformViewIds(inflatedView);

        getBottomBarView().addView(inflatedView, 1);
        inflatedView.addOnLayoutChangeListener(
                new OnLayoutChangeListener() {
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
                        inflatedView.removeOnLayoutChangeListener(this);
                        setBottomControlsHeight(getBottomBarHeight());
                    }
                });
        return true;
    }

    private static void sendPendingIntentWithUrl(
            PendingIntent pendingIntent,
            Intent extraIntent,
            Activity activity,
            Supplier<Tab> tabProvider) {
        Intent addedIntent = extraIntent == null ? new Intent() : new Intent(extraIntent);
        Tab tab = tabProvider.get();
        if (tab != null) addedIntent.setData(Uri.parse(tab.getUrl().getSpec()));
        try {
            ActivityOptions options = ActivityOptions.makeBasic();
            ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartMode(options);
            pendingIntent.send(activity, 0, addedIntent, null, null, null, options.toBundle());
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

    // BrowserControlsStateProvider.Observer methods

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        if (mBottomBarView != null) {
            int minHeight = mBrowserControlsSizer.getBottomControlsMinHeight();
            mBottomBarView.setTranslationY(bottomOffset - minHeight);
        }
        // If the bottom bar is not visible use the top controls as a guide to set state.
        int offset = getBottomBarHeight() == 0 ? topOffset : bottomOffset;
        int height =
                getBottomBarHeight() == 0
                        ? mBrowserControlsSizer.getTopControlsHeight()
                        : mBrowserControlsSizer.getBottomControlsHeight();
        // Avoid spamming this callback across process boundaries, by only sending messages at
        // absolute transitions.
        if (Math.abs(offset) == height || offset == 0) {
            CustomTabsConnection.getInstance()
                    .onBottomBarScrollStateChanged(mDataProvider.getSession(), offset != 0);
        }
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        if (!isViewReady()) return;
        // Bottom offset might not have been received by BrowserControlsManager at this point, so
        // using getBrowserControlHiddenRatio(), http://crbug.com/928903.
        getBottomBarView()
                .setTranslationY(
                        mBrowserControlsSizer.getBrowserControlHiddenRatio() * bottomControlsHeight
                                - mBrowserControlsSizer.getBottomControlsMinHeightOffset());
    }

    /**
     * This method temporarily hides bottomBarView.
     *
     * <p>If you need to remove bottom bar completely use {@link #hideBottomBar()}.
     *
     * @param hidesBottomBar whether bottom bar needs to be hidden.
     */
    public void hideBottomBar(boolean hidesBottomBar) {
        if (hidesBottomBar) {
            // No-op if it is already in hidden state. This keeps bottom controls height from
            // changing inadvertently while it is being updated by other insets.
            if (getBottomBarView().getVisibility() == View.GONE) return;
            getBottomBarView().setVisibility(View.GONE);
            setBottomControlsHeight(0);
        } else {
            getBottomBarView().setVisibility(View.VISIBLE);
            setBottomControlsHeight(getBottomBarHeight());
        }
    }

    private void onViewportInsetChange(ViewportInsets insets) {
        if (mBottomBarView == null) return;
        boolean isKeyboardShowing =
                mWindowAndroid
                        .getKeyboardDelegate()
                        .isKeyboardShowing(mBottomBarView.getContext(), mBottomBarView);

        hideBottomBar(insets.viewVisibleHeightInset > 0 || isKeyboardShowing);
    }

    /**
     * Starts listening for swipe up gesture to send the {@link PendingIntent}.
     * @param pendingIntent The {@link PendingIntent} to be sent.
     */
    private void startListeningForSwipeUpGestures(PendingIntent pendingIntent) {
        if (mBottomBarView == null) return;
        mSwipeUpPendingIntent = pendingIntent;
        mBottomBarView.setSwipeHandler(this);
    }

    private void stopListeningForSwipeUpGestures() {
        if (mBottomBarView == null) return;
        mBottomBarView.setSwipeHandler(null);
        mSwipeUpPendingIntent = null;
    }

    private void setBottomControlsHeight(int height) {
        int minHeight = mBrowserControlsSizer.getBottomControlsMinHeight();
        mBrowserControlsSizer.setBottomControlsHeight(minHeight + height, minHeight);
    }

    // SwipeGestureListener.SwipeHandler methods

    @Override
    public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
        if (mSwipeUpPendingIntent == null) return;
        // Do not send URL for swipe action.
        sendPendingIntentWithUrl(mSwipeUpPendingIntent, null, mActivity, () -> null);
    }

    @Override
    public boolean isSwipeEnabled(@ScrollDirection int direction) {
        return direction == ScrollDirection.UP
                && getBottomBarView().getVisibility() == View.VISIBLE;
    }

    void setBottomBarViewForTesting(CustomTabBottomBarView bottomBarView) {
        mBottomBarView = bottomBarView;
    }
}
