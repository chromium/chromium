// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Runs a page's {@code beforeunload} handler before its tab is closed and reports the answer back
 * to the closure.
 *
 * <p>{@link TabRemover.TabClosePrompter} is the seam the shared closure walk calls through, and
 * every remover that serves a live {@link TabModel} builds one. A tab with no {@link WebContents}
 * -- an archived tab, a tab that has never been restored -- has no renderer to answer, so it is not
 * asked and the walk moves straight on.
 */
@NullMarked
public class BeforeUnloadTabClosePrompter implements TabRemover.TabClosePrompter {
    @Override
    public boolean prompt(
            TabClosureParams tabClosureParams, Tab tab, Runnable onProceed, Runnable onCancel) {
        WebContents webContents = webContentsNeedingBeforeUnload(tabClosureParams, tab);
        if (webContents == null) return false;

        // The closure owns the teardown, so the completion this dispatch produces must not let
        // Content close the page on its own.
        tab.setSuppressBeforeUnloadAutoClose(/* suppress= */ true);
        tab.addObserver(new BeforeUnloadResultObserver(tab, webContents, onProceed, onCancel));
        webContents.dispatchBeforeUnload(/* autoCancel= */ false);
        return true;
    }

    /**
     * Returns the {@link WebContents} that must run {@code beforeunload} before {@code tab} closes,
     * or null when nothing needs to run.
     */
    private static @Nullable WebContents webContentsNeedingBeforeUnload(
            TabClosureParams tabClosureParams, Tab tab) {
        if (!tabClosureParams.allowUnloadHandlers
                || !TabClosureParamsUtils.areUnloadHandlersEnabled()
                || tab.isDestroyed()) {
            return null;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;

        // Only a beforeunload handler can ask the user to stay. Unload, pagehide and
        // visibilitychange handlers do not run as part of the dispatch; they run when the tab is
        // torn down.
        return webContents.needToFireBeforeUnload() ? webContents : null;
    }

    /**
     * Resolves a single {@code beforeunload} dispatch, whichever way it ends, and detaches itself
     * once it has.
     *
     * <p>A dispatch ends with a completion from the renderer, or it is abandoned. It is abandoned
     * when the tab is destroyed, when its primary page changes, when the {@link WebContents} leaves
     * the tab, or when the tab leaves its window, among others. Each of those cancels: no
     * completion will arrive, and a dispatch left unanswered stalls the closure waiting on it.
     *
     * <p>These are the abandonment signals a {@link TabObserver} can see, which is narrower than
     * the set the native suppression mark drops on. A tab showing a native page does not report a
     * renderer that goes away, so such a crash reaches neither this observer nor a completion.
     *
     * <p>A same-document navigation is deliberately not an abandonment: the page stays, so the
     * dispatch still completes.
     *
     * <p>This observer answers the first completion the tab reports, which is the dispatch's own
     * while it is the only one in flight. A concurrent dispatch -- a back press, a Custom Tab close
     * -- breaks that pairing.
     */
    private static class BeforeUnloadResultObserver implements TabObserver {
        private final Tab mTab;
        private final WebContents mWebContents;
        private final Runnable mOnProceed;
        private final Runnable mOnCancel;
        private boolean mResolved;

        BeforeUnloadResultObserver(
                Tab tab, WebContents webContents, Runnable onProceed, Runnable onCancel) {
            mTab = tab;
            mWebContents = webContents;
            mOnProceed = onProceed;
            mOnCancel = onCancel;
        }

        @Override
        public void onBeforeUnloadFired(Tab tab, boolean proceed) {
            resolve(proceed);
        }

        @Override
        public void onDestroyed(Tab tab) {
            resolve(/* proceed= */ false);
        }

        @Override
        public void onCrash(Tab tab) {
            // A crashed page has no unsaved state left to protect, so the tab closes. Refusing
            // here would leave the user unable to close a crashed tab, and would abandon a whole
            // close-all over one of them. Desktop's UnloadController::RenderProcessGone likewise
            // proceeds.
            resolve(/* proceed= */ true);
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
            // TabObserver's default implementation detaches this observer when the tab leaves its
            // window. Resolving first is what keeps the closure from waiting on an answer that can
            // no longer reach it; resolve() detaches this observer itself.
            if (window == null) {
                resolve(/* proceed= */ false);
            }
        }

        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
            if (navigation.hasCommitted() && !navigation.isSameDocument()) {
                resolve(/* proceed= */ false);
            }
        }

        @Override
        public void onContentChanged(Tab tab) {
            // This also fires for changes that leave the dispatch alone -- a custom view being
            // set, a native page being shown or hidden, a deferred content view being inflated.
            // Only the WebContents actually going away ends the dispatch.
            if (tab.getWebContents() != mWebContents) {
                resolve(/* proceed= */ false);
            }
        }

        private void resolve(boolean proceed) {
            if (mResolved) return;
            mResolved = true;
            mTab.removeObserver(this);

            // A completion delivered from the renderer arrives with
            // RenderFrameHostImpl::ProcessBeforeUnloadCompletedFromFrame still on the stack, and
            // it touches the frame tree again after Java returns, so the closure continues on a
            // fresh task rather than on that stack. Posting also keeps a batch from nesting each
            // tab's closure inside the previous tab's dispatch.
            PostTask.postTask(TaskTraits.UI_DEFAULT, proceed ? mOnProceed : mOnCancel);
        }
    }
}
