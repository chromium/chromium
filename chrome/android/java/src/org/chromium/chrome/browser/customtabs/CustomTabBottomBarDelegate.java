// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.net.Uri;
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

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.RemoteViewsWithNightModeInflater;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.util.List;

import javax.inject.Inject;

/**
 * Delegate that manages bottom bar area inside of {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabBottomBarDelegate implements BrowserControlsStateProvider.Observer {
    private static final String TAG = "CustomTab";
    private static final int SLIDE_ANIMATION_DURATION_MS = 400;

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ObservableSupplier<Integer> mAutofillUiBottomInsetSupplier;
    private final BrowserServicesIntentDataProvider mDataProvider;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabNightModeStateController mNightModeStateController;
    private final SystemNightModeMonitor mSystemNightModeMonitor;

    private ViewGroup mBottomBarView;
    @Nullable private View mBottomBarContentView;
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
            int originalId = v.getId();
            if (ChromeFeatureList.sCctRemoveRemoteViewIds.isEnabled()) {
                originalId = (Integer) v.getTag(R.id.view_id_tag_key);
            }
            extraIntent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_CLICKED_ID, originalId);
            sendPendingIntentWithUrl(mClickPendingIntent, extraIntent, mActivity, mTabProvider);
        }
    };

    @Inject
    public CustomTabBottomBarDelegate(Activity activity, WindowAndroid windowAndroid,
            BrowserServicesIntentDataProvider dataProvider,
            BrowserControlsSizer browserControlsSizer,
            ObservableSupplier<Integer> autofillUiBottomInsetSupplier,
            CustomTabNightModeStateController nightModeStateController,
            SystemNightModeMonitor systemNightModeMonitor, CustomTabActivityTabProvider tabProvider,
            CustomTabCompositorContentInitializer compositorContentInitializer) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mDataProvider = dataProvider;
        mBrowserControlsSizer = browserControlsSizer;
        mAutofillUiBottomInsetSupplier = autofillUiBottomInsetSupplier;
        mNightModeStateController = nightModeStateController;
        mSystemNightModeMonitor = systemNightModeMonitor;
        mTabProvider = tabProvider;
        browserControlsSizer.addObserver(this);

        compositorContentInitializer.addCallback(this::addOverlayPanelManagerObserver);

        Callback<Integer> insetObserver = this::onViewPortInsetChange;
        mWindowAndroid.getApplicationBottomInsetProvider().addObserver(insetObserver);
        mAutofillUiBottomInsetSupplier.addObserver(insetObserver);
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
                    mBrowserControlsSizer.setBottomControlsHeight(getBottomBarHeight(), 0);
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

    public void addOverlayPanelManagerObserver(LayoutManagerImpl layoutDriver) {
        layoutDriver.getOverlayPanelManager().addObserver(new OverlayPanelManagerObserver() {
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

    /**
     * This method remove bottomBarView completely.
     * If you need to hide it temporarily use {@link #hideBottomBar(boolean)}.
     */
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
        mBrowserControlsSizer.setBottomControlsHeight(0, 0);
    }

    private void transformViewIds(View view) {
        // Store the old id in a tag. The tag key here does not matter as long
        // as it is unique across all tags.
        view.setTag(R.id.view_id_tag_key, view.getId());
        view.setId(View.NO_ID);
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            final int childCount = group.getChildCount();
            for (int i = 0; i < childCount; i++) {
                final View child = group.getChildAt(i);
                transformViewIds(child);
            }
        }
    }

    private boolean showRemoteViews(RemoteViews remoteViews) {
        final View inflatedView = RemoteViewsWithNightModeInflater.inflate(remoteViews,
                getBottomBarView(), mNightModeStateController.isInNightMode(),
                mSystemNightModeMonitor.isSystemNightModeOn());

        if (inflatedView == null) return false;

        if (mClickableIDs != null && mClickPendingIntent != null) {
            for (int id : mClickableIDs) {
                if (id < 0) return false;
                View view = inflatedView.findViewById(id);
                if (view != null) view.setOnClickListener(mBottomBarClickListener);
            }
        }
        if (ChromeFeatureList.sCctRemoveRemoteViewIds.isEnabled()) {
            // Set all views' ids to be View.NO_ID to prevent them clashing with
            // chrome's resource ids. See http://crbug.com/1061872
            transformViewIds(inflatedView);
        }
        getBottomBarView().addView(inflatedView, 1);
        inflatedView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                inflatedView.removeOnLayoutChangeListener(this);
                mBrowserControlsSizer.setBottomControlsHeight(getBottomBarHeight(), 0);
            }
        });
        return true;
    }

    private static void sendPendingIntentWithUrl(PendingIntent pendingIntent, Intent extraIntent,
            Activity activity, CustomTabActivityTabProvider tabProvider) {
        Intent addedIntent = extraIntent == null ? new Intent() : new Intent(extraIntent);
        Tab tab = tabProvider.getTab();
        if (tab != null) addedIntent.setData(Uri.parse(tab.getUrl().getSpec()));
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

    // BrowserControlsStateProvider.Observer methods

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        if (mBottomBarView != null) mBottomBarView.setTranslationY(bottomOffset);
        // If the bottom bar is not visible use the top controls as a guide to set state.
        int offset = getBottomBarHeight() == 0 ? topOffset : bottomOffset;
        int height = getBottomBarHeight() == 0 ? mBrowserControlsSizer.getTopControlsHeight()
                                               : mBrowserControlsSizer.getBottomControlsHeight();
        // Avoid spamming this callback across process boundaries, by only sending messages at
        // absolute transitions.
        if (Math.abs(offset) == height || offset == 0) {
            CustomTabsConnection.getInstance().onBottomBarScrollStateChanged(
                    mDataProvider.getSession(), offset != 0);
        }
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        if (!isViewReady()) return;
        // Bottom offset might not have been received by BrowserControlsManager at this point, so
        // using getBrowserControlHiddenRatio(), http://crbug.com/928903.
        getBottomBarView().setTranslationY(
                mBrowserControlsSizer.getBrowserControlHiddenRatio() * bottomControlsHeight);
    }

    /**
     * This method temporarily hides bottomBarView.
     *
     * If you need to remove bottom bar completely use {@link #hideBottomBar()}.
     *
     * @param hidesBottomBar whether bottom bar needs to be hidden.
     */
    public void hideBottomBar(boolean hidesBottomBar) {
        if (hidesBottomBar) {
            getBottomBarView().setVisibility(View.GONE);
            mBrowserControlsSizer.setBottomControlsHeight(0, 0);
        } else {
            getBottomBarView().setVisibility(View.VISIBLE);
            mBrowserControlsSizer.setBottomControlsHeight(getBottomBarHeight(), 0);
        }
    }

    private void onViewPortInsetChange(Integer integer) {
        if (mBottomBarView == null) return;
        hideBottomBar(hasNonZeroInset(mAutofillUiBottomInsetSupplier)
                || hasNonZeroInset(mWindowAndroid.getApplicationBottomInsetProvider()));
    }

    private static boolean hasNonZeroInset(Supplier<Integer> insetSupplier) {
        Integer inset = insetSupplier.get();
        return inset != null && inset > 0;
    }
}
