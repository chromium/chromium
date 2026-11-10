// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.SmoothTransitionDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

/**
 * Owns the {@link NativePage} shown by a {@link TabImpl} and its show/hide/freeze/unfreeze
 * lifecycle. Owned by {@link TabImpl}, shares its lifetime, and holds a back-reference to it. This
 * class is UI-thread-only (all callers of {@link Tab#getNativePage()} and lifecycle methods run on
 * the UI thread; mutating lifecycle methods assert {@link ThreadUtils#assertOnUiThread()}).
 */
@NullMarked
final class TabNativePageController {
    private final TabImpl mTab;
    private final OnAttachStateChangeListener mAttachStateChangeListener;

    /**
     * True after a native page has been hidden, before a new background color has been explicitly
     * set. This is useful when the implicit background color (previously set before showing the
     * native page) is no longer necessarily relevant.
     */
    private boolean mWaitingOnBgColorAfterHidingNativePage;

    /**
     * Variables used to track native page creation prior to mNativePage assignment. Avoids the case
     * where native pages can unintentionally re-create themselves by calling {@link
     * NativePage#onStateChange} during the creation process.
     */
    private boolean mIsAlreadyCreatingNativePage;

    /** The current native page (e.g. chrome-native://newtab), or {@code null} if there is none. */
    private @Nullable NativePage mNativePage;

    /**
     * The previous native page during a transition, held so observers can inspect the new state
     * before the previous page is destroyed or so it can be cleaned up if the tab is destroyed.
     */
    private @Nullable NativePage mPreviousNativePage;

    private @Nullable String mPendingNativePageHost;

    private @Nullable NativePage mFreezingNativePage;

    private @Nullable ActivityStateListener mDetachedNativePageFreezeListener;

    private @Nullable SmoothTransitionDelegate mNativePageSmoothTransitionDelegate;

    TabNativePageController(TabImpl tab, OnAttachStateChangeListener attachStateChangeListener) {
        mTab = tab;
        mAttachStateChangeListener = attachStateChangeListener;
    }

    @Nullable NativePage getNativePage() {
        return mNativePage;
    }

    void onWebContentBackgroundColorChanged() {
        ThreadUtils.assertOnUiThread();
        mWaitingOnBgColorAfterHidingNativePage = false;
    }

    void onDidFirstVisuallyNonEmptyPaint() {
        ThreadUtils.assertOnUiThread();
        if (mWaitingOnBgColorAfterHidingNativePage) {
            assert !mTab.isDestroyed();
            mWaitingOnBgColorAfterHidingNativePage = false;
            mTab.onBackgroundColorChanged();
        }
    }

    boolean cancelSmoothTransition() {
        ThreadUtils.assertOnUiThread();
        SmoothTransitionDelegate delegate = mNativePageSmoothTransitionDelegate;
        if (delegate == null) return false;
        mNativePageSmoothTransitionDelegate = null;
        delegate.cancel();
        return true;
    }

    boolean startSmoothTransition(Runnable onShown) {
        ThreadUtils.assertOnUiThread();
        SmoothTransitionDelegate delegate = mNativePageSmoothTransitionDelegate;
        if (delegate == null) return false;
        mNativePageSmoothTransitionDelegate = null;
        delegate.start(onShown);
        return true;
    }

    void onViewDetachedFromWindow(View view) {
        ThreadUtils.assertOnUiThread();
        if (hasLiveNativePage() && mNativePage.getView() == view && !cancelSmoothTransition()) {
            // reset ntp view state.
            view.setAlpha(1f);
        }
    }

    void onAttachToWindow() {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed()) return;
        unregisterAndClearDetachedNativePageFreezeListener();
        if (mNativePage == null) return;
        // Reload or freeze the NativePage (if any), since the old NativePage holds a reference to
        // the old Activity. If hidden, detach its view and freeze the native page to avoid eager
        // instantiation of background native pages. Unfrozen PDF pages are exempted from freezing
        // because freezing destroys the PdfPage and removes the ChromePdfViewerFragment from
        // FragmentManager, losing unsaved annotations. Instead, reloading via
        // maybeShowNativePage() allows the new PdfCoordinator to reclaim the restored fragment by
        // tag and relocate misplaced fragment views before destroying the old page. Already-frozen
        // PDF pages remain frozen.
        detachNativePageView();
        NativePage nativePage = mNativePage;
        if (mTab.isDestroyed() || nativePage == null) return;
        if (mTab.isHidden() && (!nativePage.isPdf() || nativePage.isFrozen())) {
            freezeNativePage();
        } else {
            maybeShowNativePage(
                    mTab.getUrl().getSpec(),
                    /* forceReload= */ true,
                    PdfUtils.getPdfInfo(nativePage));
        }
    }

    void onDetachFromWindow(@Nullable Activity activity) {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed()) return;
        unregisterAndClearDetachedNativePageFreezeListener();
        if (hasLiveNativePage() && !tryDeferDetachedNativePageFreeze(activity)) {
            freezeDetachedNativePage();
        }
    }

    void prepareForDestroy() {
        ThreadUtils.assertOnUiThread();
        unregisterAndClearDetachedNativePageFreezeListener();
        detachNativePageView();
    }

    void destroy() {
        ThreadUtils.assertOnUiThread();
        assert mTab.isDestroyed();
        unregisterAndClearDetachedNativePageFreezeListener();
        mWaitingOnBgColorAfterHidingNativePage = false;
        cancelSmoothTransition();
        destroyPreviousNativePage();
        NativePage nativePage = mNativePage;
        mNativePage = null;
        if (nativePage != null) {
            if (!nativePage.isFrozen()) {
                View view = nativePage.getView();
                if (view != null) {
                    view.removeOnAttachStateChangeListener(mAttachStateChangeListener);
                    UiUtils.removeViewFromParent(view);
                }
            }
            destroyNativePageInternal(nativePage);
        }
    }

    /**
     * Hides and destroys the native page if it was being shown.
     *
     * @param notify {@code true} to trigger {@link TabObserver#onContentChanged} event.
     */
    void hideNativePage(boolean notify) {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed() || mNativePage == null) return;
        unregisterAndClearDetachedNativePageFreezeListener();
        destroyPreviousNativePage();
        mPreviousNativePage = detachCurrentNativePage();
        mWaitingOnBgColorAfterHidingNativePage = true;
        if (notify) {
            mTab.notifyContentChanged();
        }
        destroyPreviousNativePage();
    }

    /**
     * Detaches the current native page, if any, from this tab and cancels any active transition
     * delegate.
     */
    private @Nullable NativePage detachCurrentNativePage() {
        ThreadUtils.assertOnUiThread();
        SmoothTransitionDelegate delegate = mNativePageSmoothTransitionDelegate;
        mNativePageSmoothTransitionDelegate = null;
        if (delegate != null) {
            delegate.cancel();
        } else if (hasLiveNativePage()) {
            View view = mNativePage.getView();
            if (view != null) {
                view.setAlpha(1.f);
            }
        }
        NativePage previousNativePage = mNativePage;
        if (mNativePage != null) {
            if (hasLiveNativePage()) {
                View view = mNativePage.getView();
                if (view != null) {
                    view.removeOnAttachStateChangeListener(mAttachStateChangeListener);
                }
            }
            mNativePage = null;
            mWaitingOnBgColorAfterHidingNativePage = true;
        }
        return previousNativePage;
    }

    @VisibleForTesting
    void showNativePage(NativePage nativePage) {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed() || mNativePage == nativePage) return;
        unregisterAndClearDetachedNativePageFreezeListener();
        destroyPreviousNativePage();
        mPreviousNativePage = detachCurrentNativePage();
        mNativePage = nativePage;
        mWaitingOnBgColorAfterHidingNativePage = false;
        if (!nativePage.isFrozen()) {
            View view = nativePage.getView();
            if (view != null) {
                view.addOnAttachStateChangeListener(mAttachStateChangeListener);
            }
        }
        if (mTab.isDisplayingBackForwardAnimation()) {
            SmoothTransitionDelegate delegate = nativePage.enableSmoothTransition();
            if (delegate != null) {
                mNativePageSmoothTransitionDelegate = delegate;
                delegate.prepare();
            }
        }
        mTab.pushNativePageStateToNavigationEntry();
        mTab.onBackgroundColorChanged();
        if (!mTab.isDestroyed() && mNativePage == nativePage) {
            mTab.updateThemeColor(TabState.UNSPECIFIED_THEME_COLOR);
        }
        if (!mTab.isDestroyed() && mNativePage == nativePage) {
            mTab.notifyContentChanged();
        }
        destroyPreviousNativePage();
    }

    boolean maybeShowNativePage(
            @Nullable String url, boolean forceReload, @Nullable PdfInfo pdfInfo) {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed()) return false;
        // While detached for reparenting we don't have an owning Activity, or TabModelSelector,
        // so we can't create the native page. The native page will be created once reparenting is
        // completed.
        if (mTab.isDetachedFromActivity()) return false;
        // TODO(crbug.com/40943608): Remove the assert after determining why WebContents can be
        // null.
        WebContents webContents = mTab.getWebContents();
        assert webContents != null;
        if (webContents == null) return false;
        // If the given url is null, there's no work to do.
        if (url == null) return false;

        // We might be in the middle of loading a native page, in that case we should bail to avoid
        // recreating another instance.
        String nativePageHost = Uri.parse(url).getHost();
        if (mIsAlreadyCreatingNativePage
                && TextUtils.equals(mPendingNativePageHost, nativePageHost)) {
            return true;
        }

        mPendingNativePageHost = nativePageHost;
        mIsAlreadyCreatingNativePage = true;

        // Prefer reusing the NativePage when loading pdf. Never offer a frozen page as the reuse
        // candidate: it has no view, and NativePageFactory.createNativePageForCustomTab() would
        // hand it straight back on URL match, leaving the tab with a frozen NativePage and a null
        // view. See crbug.com/568278676.
        NativePage candidateForReuse =
                hasLiveNativePage()
                                && (!forceReload
                                        || (pdfInfo != null
                                                && mNativePage.isPdf()
                                                && PdfUtils.isReuseFragmentEnabled()))
                        ? mNativePage
                        : null;

        TabDelegateFactory delegateFactory = assertNonNull(mTab.getDelegateFactory());
        NativePage nativePage =
                delegateFactory.createNativePage(url, candidateForReuse, mTab, pdfInfo);

        mIsAlreadyCreatingNativePage = false;
        mPendingNativePageHost = null;

        if (mTab.isDestroyed()) {
            if (nativePage != null
                    && nativePage != candidateForReuse
                    && nativePage != mNativePage) {
                if (!nativePage.isFrozen()) {
                    View view = nativePage.getView();
                    if (view != null) {
                        UiUtils.removeViewFromParent(view);
                    }
                }
                destroyNativePageInternal(nativePage);
            }
            return false;
        }
        if (nativePage == null || (nativePage == candidateForReuse && nativePage != mNativePage)) {
            return false;
        }

        showNativePage(nativePage);
        if (!mTab.isDestroyed() && mNativePage == nativePage) {
            mTab.notifyPageTitleChanged();
        }
        if (!mTab.isDestroyed() && mNativePage == nativePage) {
            mTab.notifyFaviconChanged();
        }
        return true;
    }

    /**
     * Unfreezes the current {@link NativePage} if it was frozen while in the background.
     *
     * @return Whether the {@link NativePage} was frozen and an unfreeze was triggered.
     */
    boolean maybeUnfreezeNativePage() {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed()) return false;
        NativePage nativePage = mNativePage;
        if (nativePage != null && nativePage.isFrozen()) {
            return maybeShowNativePage(
                    nativePage.getUrl(), /* forceReload= */ true, PdfUtils.getPdfInfo(nativePage));
        }
        return false;
    }

    void freezeNativePage() {
        ThreadUtils.assertOnUiThread();
        if (mTab.isDestroyed() || !hasLiveNativePage() || mFreezingNativePage != null) return;
        NativePage pageToFreeze = mNativePage;
        View view = pageToFreeze.getView();
        if (view != null && view.getParent() != null) return;
        unregisterAndClearDetachedNativePageFreezeListener();
        if (view != null) {
            view.removeOnAttachStateChangeListener(mAttachStateChangeListener);
        }
        cancelSmoothTransition();
        mFreezingNativePage = pageToFreeze;
        FrozenNativePage frozenNativePage = FrozenNativePage.freeze(pageToFreeze);
        mFreezingNativePage = null;
        if (mTab.isDestroyed() || mNativePage != pageToFreeze) {
            frozenNativePage.destroy();
            return;
        }
        mNativePage = frozenNativePage;
        mTab.updateInteractableState();
    }

    private void destroyPreviousNativePage() {
        NativePage previous = mPreviousNativePage;
        mPreviousNativePage = null;
        destroyNativePageInternal(previous);
    }

    private void destroyNativePageInternal(@Nullable NativePage nativePage) {
        if (nativePage == null || nativePage == mFreezingNativePage) return;
        assert nativePage != mNativePage : "Attempting to destroy active page.";

        nativePage.destroy();
    }

    /** Returns whether {@link #mNativePage} is set and not frozen (i.e. it has a live view). */
    @EnsuresNonNullIf("mNativePage")
    private boolean hasLiveNativePage() {
        return mNativePage != null && !mNativePage.isFrozen();
    }

    private void freezeDetachedNativePage() {
        // Since mIsDetachedFromActivity is now true, getView() returns null while
        // isNativePage() remains true. Update interactability first so NativePage
        // observers (e.g. NtpFeedSurfaceLifecycleManager) can save UI state while the
        // view hierarchy is still attached to the window.
        // PDF pages are exempted from freezing because freezing destroys the PdfPage and
        // loses unsaved annotations, but their view is still detached from the Activity.
        // Notify observers afterwards so they observe the final NativePage state
        // (e.g. ToolbarPhone must not keep NTP layout for a frozen NewTabPage) and
        // CompositorViewHolder reclaims focus.
        mTab.updateInteractableState();
        if (mTab.isDestroyed()) return;
        detachNativePageView();
        if (mTab.isDestroyed()) return;
        if (mNativePage != null && !mNativePage.isPdf()) {
            freezeNativePage();
        }
        if (!mTab.isDestroyed()) {
            mTab.notifyContentChanged();
        }
    }

    private void unregisterAndClearDetachedNativePageFreezeListener() {
        ThreadUtils.assertOnUiThread();
        if (mDetachedNativePageFreezeListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mDetachedNativePageFreezeListener);
            mDetachedNativePageFreezeListener = null;
        }
    }

    private void detachNativePageView() {
        ThreadUtils.assertOnUiThread();
        if (!hasLiveNativePage()) return;
        View view = mNativePage.getView();
        if (view != null) {
            UiUtils.removeViewFromParent(view);
        }
    }

    private boolean tryDeferDetachedNativePageFreeze(@Nullable Activity activity) {
        if (!ChromeFeatureList.sDeferDetachedNativePageFreeze.isEnabled()
                || activity == null
                || !ApplicationStatus.isInitialized()
                || ApplicationStatus.getStateForActivity(activity) >= ActivityState.STOPPED
                || mTab.isHidden()) {
            return false;
        }
        // Defer freezing the NativePage until the old Activity is stopped. This prevents visual
        // glitches (e.g. blank NTP or misdrawn ToolbarPhone) while the old Activity is still
        // visible before being torn down or paused.
        mDetachedNativePageFreezeListener =
                (_, newState) -> {
                    ThreadUtils.assertOnUiThread();
                    if (newState >= ActivityState.STOPPED) {
                        unregisterAndClearDetachedNativePageFreezeListener();
                        if (mTab.isDestroyed()) return;
                        if (mTab.isDetachedFromActivity() && hasLiveNativePage()) {
                            freezeDetachedNativePage();
                        }
                    }
                };
        ApplicationStatus.registerStateListenerForActivity(
                mDetachedNativePageFreezeListener, activity);
        return true;
    }
}
