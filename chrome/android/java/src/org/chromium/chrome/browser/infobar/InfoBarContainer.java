// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.banners.SwipableOverlayView;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.util.ArrayList;

/**
 * A container for all the infobars of a specific tab.
 * Note that infobars creation can be initiated from Java of from native code.
 * When initiated from native code, special code is needed to keep the Java and native infobar in
 * sync, see NativeInfoBar.
 */
public class InfoBarContainer extends SwipableOverlayView implements UserData {
    private static final String TAG = "InfoBarContainer";

    private static final Class<InfoBarContainer> USER_DATA_KEY = InfoBarContainer.class;

    /** Top margin, including the toolbar and tabstrip height and 48dp of web contents. */
    private static final int TOP_MARGIN_PHONE_DP = 104;
    private static final int TOP_MARGIN_TABLET_DP = 144;

    /** Length of the animation to fade the InfoBarContainer back into View. */
    private static final long REATTACH_FADE_IN_MS = 250;

    /** Whether or not the InfoBarContainer is allowed to hide when the user scrolls. */
    private static boolean sIsAllowedToAutoHide = true;

    /**
     * A listener for the InfoBar animations.
     */
    public interface InfoBarAnimationListener {
        public static final int ANIMATION_TYPE_SHOW = 0;
        public static final int ANIMATION_TYPE_SWAP = 1;
        public static final int ANIMATION_TYPE_HIDE = 2;

        /**
         * Notifies the subscriber when an animation is completed.
         */
        void notifyAnimationFinished(int animationType);

        /**
         * Notifies the subscriber when all animations are finished.
         * @param frontInfoBar The frontmost infobar or {@code null} if none are showing.
         */
        void notifyAllAnimationsFinished(Item frontInfoBar);
    }

    /**
     * An observer that is notified of changes to a {@link InfoBarContainer} object.
     */
    public interface InfoBarContainerObserver {
        /**
         * Called when an {@link InfoBar} is about to be added (before the animation).
         * @param container The notifying {@link InfoBarContainer}
         * @param infoBar An {@link InfoBar} being added
         * @param isFirst Whether the infobar container was empty
         */
        void onAddInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isFirst);

        /**
         * Called when an {@link InfoBar} is about to be removed (before the animation).
         * @param container The notifying {@link InfoBarContainer}
         * @param infoBar An {@link InfoBar} being removed
         * @param isLast Whether the infobar container is going to be empty
         */
        void onRemoveInfoBar(InfoBarContainer container, InfoBar infoBar, boolean isLast);

        /**
         * Called when the InfobarContainer is attached to the window.
         * @param hasInfobars True if infobar container has infobars to show.
         */
        void onInfoBarContainerAttachedToWindow(boolean hasInfobars);

        /**
         * A notification that the shown ratio of the infobar container has changed.
         * @param container The notifying {@link InfoBarContainer}
         * @param shownRatio The shown ratio of the infobar container.
         */
        void onInfoBarContainerShownRatioChanged(InfoBarContainer container, float shownRatio);
    }

    /** Resets the state of the InfoBarContainer when the user navigates. */
    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
                boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                int httpStatusCode) {
            if (hasCommitted && isInMainFrame) {
                setHidden(false);
            }
        }

        @Override
        public void onContentChanged(Tab tab) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null && webContents != getWebContents()) {
                setWebContents(webContents);
                nativeSetWebContents(mNativeInfoBarContainer, webContents);
            }

            mTabView.removeOnAttachStateChangeListener(mAttachedStateListener);
            mTabView = tab.getView();
            mTabView.addOnAttachStateChangeListener(mAttachedStateListener);
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
            if (!isAttached) return;

            mTab = tab;
            updateLayoutParams(tab.getActivity());
            setParentView((ViewGroup) tab.getActivity().findViewById(R.id.bottom_container));
        }
    };

    /**
     * Adds/removes the {@link InfoBarContainer} when the tab's view is attached/detached. This is
     * mostly to ensure the infobars are not shown in tab switcher overview mode.
     */
    private final OnAttachStateChangeListener mAttachedStateListener =
            new OnAttachStateChangeListener() {
        @Override
        public void onViewDetachedFromWindow(View v) {
            removeFromParentView();
        }

        @Override
        public void onViewAttachedToWindow(View v) {
            addToParentView();
        }
    };

    private final InfoBarContainerLayout mLayout;

    /** Helper class to manage showing in-product help bubbles over specific info bars. */
    private final IPHInfoBarSupport mIPHSupport;

    /** Native InfoBarContainer pointer which will be set by nativeInit(). */
    private final long mNativeInfoBarContainer;

    /** The list of all InfoBars in this container, regardless of whether they've been shown yet. */
    private final ArrayList<InfoBar> mInfoBars = new ArrayList<InfoBar>();

    /** True when this container has been emptied and its native counterpart has been destroyed. */
    private boolean mDestroyed;

    /** Parent view that contains the InfoBarContainerLayout. */
    private ViewGroup mParentView;

    /** The view that {@link Tab#getView()} returns. */
    private View mTabView;

    /** Whether or not this View should be hidden. */
    private boolean mIsHidden;

    /** Animation used to snap the container to the nearest state if scroll direction changes. */
    private Animator mScrollDirectionChangeAnimation;

    /** Whether or not the current scroll is downward. */
    private boolean mIsScrollingDownward;

    /** Tracks the previous event's scroll offset to determine if a scroll is up or down. */
    private int mLastScrollOffsetY;

    /** A {@link BottomSheetObserver} so this view knows when to show/hide. */
    private BottomSheetObserver mBottomSheetObserver;

    private final ObserverList<InfoBarContainerObserver> mObservers =
            new ObserverList<InfoBarContainerObserver>();

    /** The tab that hosts this infobar container. */
    private Tab mTab;

    public static InfoBarContainer from(Tab tab) {
        InfoBarContainer container = get(tab);
        if (container == null) {
            container = tab.getUserDataHost().setUserData(USER_DATA_KEY, new InfoBarContainer(tab));
        }
        return container;
    }

    /**
     * Returns {@link InfoBarContainer} object for a given {@link Tab}, or {@code null}
     * if there is no object available.
     */
    @Nullable
    public static InfoBarContainer get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private InfoBarContainer(Tab tab) {
        super(tab.getThemedApplicationContext(), null);

        tab.addObserver(mTabObserver);
        mTabView = tab.getView();
        mTab = tab;

        // TODO(newt): move this workaround into the infobar views if/when they're scrollable.
        // Workaround for http://crbug.com/407149. See explanation in onMeasure() below.
        setVerticalScrollBarEnabled(false);

        updateLayoutParams(tab.getActivity());

        mParentView = getBottomContainer(tab);

        Runnable makeContainerVisibleRunnable = () -> runUpEventAnimation(true);
        Context context = tab.getThemedApplicationContext();
        mLayout = new InfoBarContainerLayout(context, makeContainerVisibleRunnable);
        addView(mLayout, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.WRAP_CONTENT, Gravity.CENTER_HORIZONTAL));

        mIPHSupport = new IPHInfoBarSupport(new IPHBubbleDelegateImpl(context));

        mLayout.addAnimationListener(mIPHSupport);
        addObserver(mIPHSupport);

        // Chromium's InfoBarContainer may add an InfoBar immediately during this initialization
        // call, so make sure everything in the InfoBarContainer is completely ready beforehand.
        mNativeInfoBarContainer = nativeInit();
    }

    private static ViewGroup getBottomContainer(Tab tab) {
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        Activity activity = windowAndroid == null ? null : windowAndroid.getActivity().get();
        return activity == null ? null : (ViewGroup) activity.findViewById(R.id.bottom_container);
    }

    private void updateLayoutParams(@Nullable ChromeActivity activity) {
        if (activity == null) {
            return;
        }
        LayoutParams lp = new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        int topMarginDp = activity.isTablet() ? TOP_MARGIN_TABLET_DP : TOP_MARGIN_PHONE_DP;
        lp.topMargin = DisplayUtil.dpToPx(DisplayAndroid.getNonMultiDisplay(activity), topMarginDp);
        setLayoutParams(lp);
    }

    public SnackbarManager getSnackbarManager() {
        if (mTab != null && mTab.getActivity() != null) {
            return mTab.getActivity().getSnackbarManager();
        }

        return null;
    }

    /**
     * Adds an {@link InfoBarContainerObserver}.
     * @param observer The {@link InfoBarContainerObserver} to add.
     */
    public void addObserver(InfoBarContainerObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a {@link InfoBarContainerObserver}.
     * @param observer The {@link InfoBarContainerObserver} to remove.
     */
    public void removeObserver(InfoBarContainerObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void setTranslationY(float translationY) {
        super.setTranslationY(translationY);
        float shownFraction = getHeight() > 0 ? 1f - (translationY / getHeight()) : 0;
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onInfoBarContainerShownRatioChanged(this, shownFraction);
        }
    }

    /**
     * Sets the parent {@link ViewGroup} that contains the {@link InfoBarContainer}.
     */
    public void setParentView(ViewGroup parent) {
        mParentView = parent;
        removeFromParentView();
        addToParentView();
    }

    @VisibleForTesting
    public void addAnimationListener(InfoBarAnimationListener listener) {
        mLayout.addAnimationListener(listener);
    }

    /**
     * Removes the passed in {@link InfoBarAnimationListener} from the {@link InfoBarContainer}.
     */
    public void removeAnimationListener(InfoBarAnimationListener listener) {
        mLayout.removeAnimationListener(listener);
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    @VisibleForTesting
    public boolean isAnimating() {
        return mLayout.isAnimating();
    }

    private void addToParentView() {
        super.addToParentView(mParentView);
    }

    /**
     * Adds an InfoBar to the view hierarchy.
     * @param infoBar InfoBar to add to the View hierarchy.
     */
    @CalledByNative
    private void addInfoBar(InfoBar infoBar) {
        assert !mDestroyed;
        if (infoBar == null) {
            return;
        }
        if (mInfoBars.contains(infoBar)) {
            assert false : "Trying to add an info bar that has already been added.";
            return;
        }

        // We notify observers immediately (before the animation starts).
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onAddInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        // We add the infobar immediately to mInfoBars but we wait for the animation to end to
        // notify it's been added, as tests rely on this notification but expects the infobar view
        // to be available when they get the notification.
        mInfoBars.add(infoBar);
        infoBar.setContext(getContext());
        infoBar.setInfoBarContainer(this);
        infoBar.createView();

        mLayout.addInfoBar(infoBar);
    }

    /**
     * Adds an InfoBar to the view hierarchy.
     * @param infoBar InfoBar to add to the View hierarchy.
     */
    @VisibleForTesting
    public void addInfoBarForTesting(InfoBar infoBar) {
        addInfoBar(infoBar);
    }

    /**
     * Notifies that an infobar's View ({@link InfoBar#getView}) has changed. If the infobar is
     * visible, a view swapping animation will be run.
     */
    public void notifyInfoBarViewChanged() {
        assert !mDestroyed;
        mLayout.notifyInfoBarViewChanged();
    }

    /**
     * Called by {@link InfoBar} to remove itself from the view hierarchy.
     *
     * @param infoBar InfoBar to remove from the View hierarchy.
     */
    void removeInfoBar(InfoBar infoBar) {
        assert !mDestroyed;

        if (!mInfoBars.remove(infoBar)) {
            assert false : "Trying to remove an InfoBar that is not in this container.";
            return;
        }

        // Notify observers immediately, before any animations begin.
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onRemoveInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        mLayout.removeInfoBar(infoBar);
    }

    /**
     * @return True when this container has been emptied and its native counterpart has been
     *         destroyed.
     */
    public boolean hasBeenDestroyed() {
        return mDestroyed;
    }

    @Override
    public void destroy() {
        removeFromParentView();
        setWebContents(null);

        ChromeActivity activity = mTab.getActivity();
        if (activity != null && mBottomSheetObserver != null && activity.getBottomSheet() != null) {
            activity.getBottomSheet().removeObserver(mBottomSheetObserver);
        }
        mLayout.removeAnimationListener(mIPHSupport);
        removeObserver(mIPHSupport);
        mDestroyed = true;
        if (mNativeInfoBarContainer != 0) {
            nativeDestroy(mNativeInfoBarContainer);
        }
    }

    /**
     * @return all of the InfoBars held in this container.
     */
    @VisibleForTesting
    public ArrayList<InfoBar> getInfoBarsForTesting() {
        return mInfoBars;
    }

    /**
     * @return True if the container has any InfoBars.
     */
    @CalledByNative
    public boolean hasInfoBars() {
        return !mInfoBars.isEmpty();
    }

    /**
     * @return Pointer to the native InfoBarAndroid object which is currently at the top of the
     *         infobar stack, or 0 if there are no infobars.
     */
    @CalledByNative
    private long getTopNativeInfoBarPtr() {
        if (!hasInfoBars()) return 0;
        return mInfoBars.get(0).getNativeInfoBarPtr();
    }

    /**
     * Hides or stops hiding this View/
     *
     * @param isHidden Whether this View is should be hidden.
     */
    public void setHidden(boolean isHidden) {
        mIsHidden = isHidden;
        if (isHidden) {
            setVisibility(View.GONE);
        } else {
            setVisibility(View.VISIBLE);
        }
    }

    /**
     * Sets whether the InfoBarContainer is allowed to auto-hide when the user scrolls the page.
     * Expected to be called when Touch Exploration is enabled.
     * @param isAllowed Whether auto-hiding is allowed.
     */
    public static void setIsAllowedToAutoHide(boolean isAllowed) {
        sIsAllowedToAutoHide = isAllowed;
    }

    @Override
    protected boolean isAllowedToAutoHide() {
        return sIsAllowedToAutoHide;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (!mIsHidden) {
            setVisibility(VISIBLE);
            setAlpha(0f);
            animate().alpha(1f).setDuration(REATTACH_FADE_IN_MS);
        }

        // Activity is checked first in the following block for tests.
        ChromeActivity activity = mTab.getActivity();
        if (activity != null && activity.getBottomSheet() != null && mBottomSheetObserver == null) {
            mBottomSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int sheetState) {
                    if (mTab.isHidden()) return;
                    setVisibility(sheetState == BottomSheet.SheetState.FULL ? INVISIBLE : VISIBLE);
                }
            };
            activity.getBottomSheet().addObserver(mBottomSheetObserver);
        }

        // Notify observers that the container has attached to the window.
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onInfoBarContainerAttachedToWindow(!mInfoBars.isEmpty());
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // Hide the View when the keyboard is showing.
        boolean isShowing = (getVisibility() == View.VISIBLE);
        if (KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                    getContext(), InfoBarContainer.this)) {
            if (isShowing) {
                // Set to invisible (instead of gone) so that onLayout() will be called when the
                // keyboard is dismissed.
                setVisibility(View.INVISIBLE);
            }
        } else {
            if (!isShowing && !mIsHidden) {
                setVisibility(View.VISIBLE);
            }
        }

        super.onLayout(changed, l, t, r, b);
    }

    @Override
    protected boolean shouldConsumeScroll(int scrollOffsetY, int scrollExtentY) {
        ChromeFullscreenManager manager = mTab.getActivity().getFullscreenManager();

        if (manager.getBottomControlsHeight() <= 0) return true;

        boolean isScrollingDownward = scrollOffsetY > mLastScrollOffsetY;
        boolean didDirectionChange = isScrollingDownward != mIsScrollingDownward;
        mLastScrollOffsetY = scrollOffsetY;
        mIsScrollingDownward = isScrollingDownward;

        // If the scroll changed directions, snap to a completely shown or hidden state.
        if (didDirectionChange) {
            runDirectionChangeAnimation(shouldSnapToVisibleState(scrollOffsetY));
            return false;
        }

        boolean areControlsCompletelyShown = manager.getBottomControlOffset() > 0;
        boolean areControlsCompletelyHidden = manager.areBrowserControlsOffScreen();

        if ((!mIsScrollingDownward && areControlsCompletelyShown)
                || (mIsScrollingDownward && !areControlsCompletelyHidden)) {
            return false;
        }

        return true;
    }

    @Override
    protected void runUpEventAnimation(boolean visible) {
        if (mScrollDirectionChangeAnimation != null) mScrollDirectionChangeAnimation.cancel();
        super.runUpEventAnimation(visible);
    }

    @Override
    protected boolean isIndependentlyAnimating() {
        return mScrollDirectionChangeAnimation != null;
    }

    /**
     * Run an animation when the scrolling direction of a gesture has changed (this does not mean
     * the gesture has ended).
     * @param visible Whether or not the view should be visible.
     */
    private void runDirectionChangeAnimation(boolean visible) {
        mScrollDirectionChangeAnimation = createVerticalSnapAnimation(visible);
        mScrollDirectionChangeAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mScrollDirectionChangeAnimation = null;
            }
        });
        mScrollDirectionChangeAnimation.start();
    }

    /**
     * @return The infobar in front.
     */
    @Nullable
    InfoBar getFrontInfoBar() {
        if (mInfoBars.isEmpty()) return null;
        return mInfoBars.get(0);
    }

    private native long nativeInit();
    private native void nativeSetWebContents(
            long nativeInfoBarContainerAndroid, WebContents webContents);
    private native void nativeDestroy(long nativeInfoBarContainerAndroid);
}
