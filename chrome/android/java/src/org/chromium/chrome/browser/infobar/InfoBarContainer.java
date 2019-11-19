// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;

import java.util.ArrayList;

/**
 * A container for all the infobars of a specific tab.
 * Note that infobars creation can be initiated from Java or from native code.
 * When initiated from native code, special code is needed to keep the Java and native infobar in
 * sync, see NativeInfoBar.
 */
public class InfoBarContainer implements UserData, KeyboardVisibilityListener {
    private static final String TAG = "InfoBarContainer";

    private static final Class<InfoBarContainer> USER_DATA_KEY = InfoBarContainer.class;

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
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (navigation.hasCommitted() && navigation.isInMainFrame()) {
                setHidden(false);
            }
        }

        @Override
        public void onContentChanged(Tab tab) {
            updateWebContents();
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
            if (isAttached) {
                initializeContainerView();
                updateWebContents();
            } else {
                destroyContainerView();
            }
        }
    };

    /**
     * Adds/removes the {@link InfoBarContainer} when the tab's view is attached/detached. This is
     * mostly to ensure the infobars are not shown in tab switcher overview mode.
     */
    private final View.OnAttachStateChangeListener mAttachedStateListener =
            new View.OnAttachStateChangeListener() {
                @Override
                public void onViewDetachedFromWindow(View v) {
                    if (mInfoBarContainerView == null) return;
                    mInfoBarContainerView.removeFromParentView();
                }

                @Override
                public void onViewAttachedToWindow(View v) {
                    if (mInfoBarContainerView == null) return;
                    mInfoBarContainerView.addToParentView();
                }
            };

    /** The list of all InfoBars in this container, regardless of whether they've been shown yet. */
    private final ArrayList<InfoBar> mInfoBars = new ArrayList<>();

    private final ObserverList<InfoBarContainerObserver> mObservers = new ObserverList<>();
    private final ObserverList<InfoBarAnimationListener> mAnimationListeners = new ObserverList<>();

    private final InfoBarContainerView.ContainerViewObserver mContainerViewObserver =
            new InfoBarContainerView.ContainerViewObserver() {
                @Override
                public void notifyAnimationFinished(int animationType) {
                    for (InfoBarAnimationListener listener : mAnimationListeners) {
                        listener.notifyAnimationFinished(animationType);
                    }
                }

                @Override
                public void notifyAllAnimationsFinished(Item frontInfoBar) {
                    for (InfoBarAnimationListener listener : mAnimationListeners) {
                        listener.notifyAllAnimationsFinished(frontInfoBar);
                    }
                }

                @Override
                public void onShownRatioChanged(float shownFraction) {
                    for (InfoBarContainer.InfoBarContainerObserver observer : mObservers) {
                        observer.onInfoBarContainerShownRatioChanged(
                                InfoBarContainer.this, shownFraction);
                    }
                }
            };

    /** The tab that hosts this infobar container. */
    private final Tab mTab;

    /** Native InfoBarContainer pointer which will be set by InfoBarContainerJni.get().init(). */
    private long mNativeInfoBarContainer;

    /** True when this container has been emptied and its native counterpart has been destroyed. */
    private boolean mDestroyed;

    /** Whether or not this View should be hidden. */
    private boolean mIsHidden;

    /**
     * The view that {@link Tab#getView()} returns.  It will be null when the {@link Tab} is
     * detached from a {@link ChromeActivity}.
     */
    private @Nullable View mTabView;

    /**
     * The view for this {@link InfoBarContainer}. It will be null when the {@link Tab} is detached
     * from a {@link ChromeActivity}.
     */
    private @Nullable InfoBarContainerView mInfoBarContainerView;

    /**
     * Helper class to manage showing in-product help bubbles over specific info bars. It will be
     * null when the {@link Tab} is detached from a {@link ChromeActivity}.
     */
    private @Nullable IPHInfoBarSupport mIPHSupport;

    /** A {@link BottomSheetObserver} so this view knows when to show/hide. */
    private @Nullable BottomSheetObserver mBottomSheetObserver;

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
        tab.addObserver(mTabObserver);
        mTabView = tab.getView();
        mTab = tab;

        if (tab.getActivity() != null) initializeContainerView();

        // Chromium's InfoBarContainer may add an InfoBar immediately during this initialization
        // call, so make sure everything in the InfoBarContainer is completely ready beforehand.
        mNativeInfoBarContainer = InfoBarContainerJni.get().init(InfoBarContainer.this);
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

    /**
     * Sets the parent {@link ViewGroup} that contains the {@link InfoBarContainer}.
     */
    public void setParentView(ViewGroup parent) {
        if (mInfoBarContainerView != null) mInfoBarContainerView.setParentView(parent);
    }

    @VisibleForTesting
    public void addAnimationListener(InfoBarAnimationListener listener) {
        mAnimationListeners.addObserver(listener);
    }

    /**
     * Removes the passed in {@link InfoBarAnimationListener} from the {@link InfoBarContainer}.
     */
    public void removeAnimationListener(InfoBarAnimationListener listener) {
        mAnimationListeners.removeObserver(listener);
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

        infoBar.setContext(mInfoBarContainerView.getContext());
        infoBar.setInfoBarContainer(this);

        // We notify observers immediately (before the animation starts).
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onAddInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        assert mInfoBarContainerView != null : "The container view is null when adding an InfoBar";

        // We add the infobar immediately to mInfoBars but we wait for the animation to end to
        // notify it's been added, as tests rely on this notification but expects the infobar view
        // to be available when they get the notification.
        mInfoBars.add(infoBar);

        mInfoBarContainerView.addInfoBar(infoBar);
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
        if (mInfoBarContainerView != null) mInfoBarContainerView.notifyInfoBarViewChanged();
    }

    /**
     * Sets the visibility for the {@link InfoBarContainerView}.
     * @param visibility One of {@link View#GONE}, {@link View#INVISIBLE}, or {@link View#VISIBLE}.
     */
    public void setVisibility(int visibility) {
        if (mInfoBarContainerView != null) mInfoBarContainerView.setVisibility(visibility);
    }

    /**
     * @return The visibility of the {@link InfoBarContainerView}.
     */
    public int getVisibility() {
        return mInfoBarContainerView != null ? mInfoBarContainerView.getVisibility() : View.GONE;
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

        assert mInfoBarContainerView
                != null : "The container view is null when removing an InfoBar.";
        mInfoBarContainerView.removeInfoBar(infoBar);
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
        destroyContainerView();
        mTab.removeObserver(mTabObserver);
        if (mNativeInfoBarContainer != 0) {
            InfoBarContainerJni.get().destroy(mNativeInfoBarContainer, InfoBarContainer.this);
            mNativeInfoBarContainer = 0;
        }
        mDestroyed = true;
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
     * Hides or stops hiding this View.
     *
     * @param isHidden Whether this View is should be hidden.
     */
    public void setHidden(boolean isHidden) {
        mIsHidden = isHidden;
        if (mInfoBarContainerView == null) return;
        mInfoBarContainerView.setHidden(isHidden);
    }

    /**
     * Sets whether the InfoBarContainer is allowed to auto-hide when the user scrolls the page.
     * Expected to be called when Touch Exploration is enabled.
     * @param isAllowed Whether auto-hiding is allowed.
     */
    public static void setIsAllowedToAutoHide(boolean isAllowed) {
        InfoBarContainerView.setIsAllowedToAutoHide(isAllowed);
    }

    // KeyboardVisibilityListener implementation.
    @Override
    public void keyboardVisibilityChanged(boolean isKeyboardShowing) {
        assert mInfoBarContainerView != null;
        boolean isShowing = (mInfoBarContainerView.getVisibility() == View.VISIBLE);
        if (isKeyboardShowing) {
            if (isShowing) {
                mInfoBarContainerView.setVisibility(View.INVISIBLE);
            }
        } else {
            if (!isShowing && !mIsHidden) {
                mInfoBarContainerView.setVisibility(View.VISIBLE);
            }
        }
    }

    private void updateWebContents() {
        // When the tab is detached, we don't update the InfoBarContainer web content so that it
        // stays null until the tab is attached to some ChromeActivity.
        if (mInfoBarContainerView == null) return;
        WebContents webContents = mTab.getWebContents();

        if (webContents != null && webContents != mInfoBarContainerView.getWebContents()) {
            mInfoBarContainerView.setWebContents(webContents);
            if (mNativeInfoBarContainer != 0) {
                InfoBarContainerJni.get().setWebContents(
                        mNativeInfoBarContainer, InfoBarContainer.this, webContents);
            }
        }

        if (mTabView != null) mTabView.removeOnAttachStateChangeListener(mAttachedStateListener);
        mTabView = mTab.getView();
        if (mTabView != null) mTabView.addOnAttachStateChangeListener(mAttachedStateListener);
    }

    private void initializeContainerView() {
        final ChromeActivity chromeActivity = mTab.getActivity();
        assert chromeActivity
                != null
            : "ChromeActivity should not be null when initializing InfoBarContainerView";
        mInfoBarContainerView = new InfoBarContainerView(chromeActivity, mContainerViewObserver,
                chromeActivity.getFullscreenManager(), chromeActivity.isTablet());

        mInfoBarContainerView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        if (mBottomSheetObserver == null) {
                            mBottomSheetObserver = new EmptyBottomSheetObserver() {
                                @Override
                                public void onSheetStateChanged(int sheetState) {
                                    if (mTab.isHidden()) return;
                                    mInfoBarContainerView.setVisibility(
                                            sheetState == BottomSheetController.SheetState.FULL
                                                    ? View.INVISIBLE
                                                    : View.VISIBLE);
                                }
                            };
                            mTab.getActivity().getBottomSheetController().addObserver(
                                    mBottomSheetObserver);
                        }

                        for (InfoBarContainer.InfoBarContainerObserver observer : mObservers) {
                            observer.onInfoBarContainerAttachedToWindow(!mInfoBars.isEmpty());
                        }
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });

        mInfoBarContainerView.setHidden(mIsHidden);
        setParentView(chromeActivity.findViewById(R.id.bottom_container));

        mIPHSupport = new IPHInfoBarSupport(new IPHBubbleDelegateImpl(chromeActivity));
        addAnimationListener(mIPHSupport);
        addObserver(mIPHSupport);

        mTab.getWindowAndroid().getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    private void destroyContainerView() {
        if (mIPHSupport != null) {
            removeAnimationListener(mIPHSupport);
            removeObserver(mIPHSupport);
            mIPHSupport = null;
        }

        if (mInfoBarContainerView != null) {
            mInfoBarContainerView.setWebContents(null);
            if (mNativeInfoBarContainer != 0) {
                InfoBarContainerJni.get().setWebContents(
                        mNativeInfoBarContainer, InfoBarContainer.this, null);
            }
            mInfoBarContainerView.destroy();
            mInfoBarContainerView = null;
        }

        ChromeActivity activity = mTab.getActivity();
        if (activity != null && mBottomSheetObserver != null) {
            activity.getBottomSheetController().removeObserver(mBottomSheetObserver);
        }

        mTab.getWindowAndroid().getKeyboardDelegate().removeKeyboardVisibilityListener(this);

        if (mTabView != null) {
            mTabView.removeOnAttachStateChangeListener(mAttachedStateListener);
            mTabView = null;
        }
    }

    /**
     * @return The infobar in front.
     */
    @Nullable
    InfoBar getFrontInfoBar() {
        if (mInfoBars.isEmpty()) return null;
        return mInfoBars.get(0);
    }

    /**
     * Returns true if any animations are pending or in progress.
     */
    @VisibleForTesting
    public boolean isAnimating() {
        assert mInfoBarContainerView != null;
        return mInfoBarContainerView.isAnimating();
    }

    /**
     * @return The {@link InfoBarContainerView} this class holds.
     */
    @VisibleForTesting
    public InfoBarContainerView getContainerViewForTesting() {
        return mInfoBarContainerView;
    }

    @NativeMethods
    interface Natives {
        long init(InfoBarContainer caller);
        void setWebContents(long nativeInfoBarContainerAndroid, InfoBarContainer caller,
                WebContents webContents);
        void destroy(long nativeInfoBarContainerAndroid, InfoBarContainer caller);
    }
}
