// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * A container for all the infobars of a specific tab.
 * Note that infobars creation can be initiated from Java or from native code.
 * When initiated from native code, special code is needed to keep the Java and native infobar in
 * sync, see NativeInfoBar.
 */
public class InfoBarContainer implements UserData, KeyboardVisibilityListener, InfoBar.Container {
    private static final Class<InfoBarContainer> USER_DATA_KEY = InfoBarContainer.class;

    private static final AccessibilityState.Listener sAccessibilityStateListener;

    static {
        sAccessibilityStateListener =
                (oldAccessibilityState, newAccessibilityState) -> {
                    setIsAllowedToAutoHide(
                            !newAccessibilityState.isTouchExplorationEnabled
                                    && !newAccessibilityState.isPerformGesturesEnabled);
                };
        AccessibilityState.addListener(sAccessibilityStateListener);
    }

    /** An observer that is notified of changes to a {@link InfoBarContainer} object. */
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
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDidStartNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigationHandle) {
                    // Make sure Y translation is reset on navigation.
                    if (mInfoBarContainerView != null) {
                        mInfoBarContainerView.setTranslationY(0);
                    }
                }

                @Override
                public void onDidFinishNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigation) {
                    if (navigation.hasCommitted()) {
                        setHidden(false);
                    }
                }

                @Override
                public void onContentChanged(Tab tab) {
                    updateWebContents();
                }

                @Override
                public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                    if (window != null) {
                        initializeContainerView(getActivity(tab));
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
                public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
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

    private final FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    assert !isDestroyed() : "Full screen observer is not correctly removed";
                    setIsAllowedToAutoHide(false);
                    mInfoBarContainerView.setTranslationY(0);
                }

                @Override
                public void onExitFullscreen(Tab tab) {
                    setIsAllowedToAutoHide(true);
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
     * detached from a {@link Activity}.
     */
    private @Nullable View mTabView;

    /**
     * The view for this {@link InfoBarContainer}. It will be null when the {@link Tab} is detached
     * from a {@link Activity}.
     */
    private @Nullable InfoBarContainerView mInfoBarContainerView;

    /**
     * Helper class to manage showing in-product help bubbles over specific info bars. It will be
     * null when the {@link Tab} is detached from a {@link Activity}.
     */
    private @Nullable IPHInfoBarSupport mIPHSupport;

    /** A {@link BottomSheetObserver} so this view knows when to show/hide. */
    private @Nullable BottomSheetObserver mBottomSheetObserver;

    /** */
    private BottomSheetController mBottomSheetController;

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
    public static @Nullable InfoBarContainer get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    public static void removeInfoBarContainerForTesting(Tab tab) {
        InfoBarContainer container = get(tab);
        if (container != null) {
            tab.getUserDataHost().removeUserData(USER_DATA_KEY);
            container.destroy();
        }
    }

    private InfoBarContainer(Tab tab) {
        tab.addObserver(mTabObserver);
        mTabView = tab.getView();
        mTab = tab;

        Activity activity = getActivity(tab);
        if (activity != null) initializeContainerView(activity);

        // Chromium's InfoBarContainer may add an InfoBar immediately during this initialization
        // call, so make sure everything in the InfoBarContainer is completely ready beforehand.
        mNativeInfoBarContainer = InfoBarContainerJni.get().init(InfoBarContainer.this);
    }

    private static Activity getActivity(Tab tab) {
        return tab.getWindowAndroid().getActivity().get();
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

    /** Sets the parent {@link ViewGroup} that contains the {@link InfoBarContainer}. */
    public void setParentView(ViewGroup parent) {
        if (mInfoBarContainerView != null) mInfoBarContainerView.setParentView(parent);
    }

    @VisibleForTesting
    public void addAnimationListener(InfoBarAnimationListener listener) {
        mAnimationListeners.addObserver(listener);
    }

    /** Removes the passed in {@link InfoBarAnimationListener} from the {@link InfoBarContainer}. */
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
        infoBar.setContainer(this);

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
    public void addInfoBarForTesting(InfoBar infoBar) {
        addInfoBar(infoBar);
    }

    @Override
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

    @Override
    public void removeInfoBar(InfoBar infoBar) {
        assert !mDestroyed;

        if (!mInfoBars.remove(infoBar)) {
            assert false : "Trying to remove an InfoBar that is not in this container.";
            return;
        }

        // Notify observers immediately, before any animations begin.
        for (InfoBarContainerObserver observer : mObservers) {
            observer.onRemoveInfoBar(this, infoBar, mInfoBars.isEmpty());
        }

        assert mInfoBarContainerView != null
                : "The container view is null when removing an InfoBar.";
        mInfoBarContainerView.removeInfoBar(infoBar);
    }

    @Override
    public boolean isDestroyed() {
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
     * @return InfoBarIdentifier of the InfoBar which is currently at the top of the infobar stack,
     *         or InfoBarIdentifier.INVALID if there are no infobars.
     */
    @CalledByNative
    private @InfoBarIdentifier int getTopInfoBarIdentifier() {
        if (!hasInfoBars()) return InfoBarIdentifier.INVALID;
        return mInfoBars.get(0).getInfoBarIdentifier();
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
    private static void setIsAllowedToAutoHide(boolean isAllowed) {
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
        // stays null until the tab is attached to some Activity.
        if (mInfoBarContainerView == null) return;
        WebContents webContents = mTab.getWebContents();

        if (webContents != null && webContents != mInfoBarContainerView.getWebContents()) {
            mInfoBarContainerView.setWebContents(webContents);
            if (mNativeInfoBarContainer != 0) {
                InfoBarContainerJni.get()
                        .setWebContents(
                                mNativeInfoBarContainer, InfoBarContainer.this, webContents);
            }
        }

        if (mTabView != null) mTabView.removeOnAttachStateChangeListener(mAttachedStateListener);
        mTabView = mTab.getView();
        if (mTabView != null) mTabView.addOnAttachStateChangeListener(mAttachedStateListener);
    }

    private void initializeContainerView(Activity activity) {
        BrowserControlsManager browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(mTab.getWindowAndroid());

        // Note: Doing a cast and pulling off dependencies from ChromeActivity is generally a
        // pattern we try to avoid. However, InfoBar is slated for deprecation soon, so a better
        // dependency management approach won't be used.
        ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier = null;
        if (activity instanceof ChromeActivity) {
            edgeToEdgeSupplier = ((ChromeActivity) activity).getEdgeToEdgeSupplier();
        }
        mInfoBarContainerView =
                new InfoBarContainerView(
                        activity,
                        mContainerViewObserver,
                        browserControlsManager,
                        edgeToEdgeSupplier,
                        DeviceFormFactor.isWindowOnTablet(mTab.getWindowAndroid()));
        if (browserControlsManager != null) {
            browserControlsManager.getFullscreenManager().removeObserver(mFullscreenObserver);
            browserControlsManager.getFullscreenManager().addObserver(mFullscreenObserver);
        }

        mInfoBarContainerView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        initBottomSheetObserver();
                        boolean infoBarsExist = !mInfoBars.isEmpty();

                        for (InfoBarContainer.InfoBarContainerObserver observer : mObservers) {
                            observer.onInfoBarContainerAttachedToWindow(infoBarsExist);
                        }
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });

        mInfoBarContainerView.setHidden(mIsHidden);
        setParentView(activity.findViewById(R.id.bottom_container));

        mIPHSupport = new IPHInfoBarSupport(new IPHBubbleDelegateImpl(activity, mTab));
        addAnimationListener(mIPHSupport);
        addObserver(mIPHSupport);

        mTab.getWindowAndroid().getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    private void initBottomSheetObserver() {
        if (mBottomSheetObserver != null) {
            return;
        }
        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int sheetState, int reason) {
                        if (mTab.isHidden()) return;
                        mInfoBarContainerView.setVisibility(
                                sheetState == BottomSheetController.SheetState.FULL
                                        ? View.INVISIBLE
                                        : View.VISIBLE);
                    }
                };
        mBottomSheetController = BottomSheetControllerProvider.from(mTab.getWindowAndroid());
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    private void destroyContainerView() {
        if (mIPHSupport != null) {
            removeAnimationListener(mIPHSupport);
            removeObserver(mIPHSupport);
            mIPHSupport = null;
        }

        BrowserControlsManager browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(mTab.getWindowAndroid());
        if (browserControlsManager != null) {
            browserControlsManager.getFullscreenManager().removeObserver(mFullscreenObserver);
        }

        if (mInfoBarContainerView != null) {
            mInfoBarContainerView.setWebContents(null);
            if (mNativeInfoBarContainer != 0) {
                InfoBarContainerJni.get()
                        .setWebContents(mNativeInfoBarContainer, InfoBarContainer.this, null);
            }
            mInfoBarContainerView.destroy();
            mInfoBarContainerView = null;
        }

        Activity activity = getActivity(mTab);
        if (activity != null && mBottomSheetObserver != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }

        mTab.getWindowAndroid().getKeyboardDelegate().removeKeyboardVisibilityListener(this);

        if (mTabView != null) {
            mTabView.removeOnAttachStateChangeListener(mAttachedStateListener);
            mTabView = null;
        }
    }

    @Override
    public boolean isFrontInfoBar(InfoBar infoBar) {
        if (mInfoBars.isEmpty()) return false;
        return mInfoBars.get(0) == infoBar;
    }

    /** Returns true if any animations are pending or in progress. */
    @VisibleForTesting
    public boolean isAnimating() {
        assert mInfoBarContainerView != null;
        return mInfoBarContainerView.isAnimating();
    }

    /**
     * @return The {@link InfoBarContainerView} this class holds.
     */
    public InfoBarContainerView getContainerViewForTesting() {
        return mInfoBarContainerView;
    }

    @NativeMethods
    interface Natives {
        long init(InfoBarContainer caller);

        void setWebContents(
                long nativeInfoBarContainerAndroid,
                InfoBarContainer caller,
                WebContents webContents);

        void destroy(long nativeInfoBarContainerAndroid, InfoBarContainer caller);
    }
}
