// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.TraceEvent;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * Hosts chrome://tabstrip-ntb.top-chrome/ in a {@link WebContents} whose compositor layer is placed
 * inside the tab strip scene in place of the native new tab button.
 *
 * <p>The WebContents and TabStripService are owned by the native TabStripNtbWebUiHost. A
 * transparent {@link ContentView} sized to the new tab button rect is added on top of the tab strip
 * so that touch, mouse and hover events in that area are routed to the WebUI; everything else falls
 * through to the native strip.
 */
@JNINamespace("android")
@NullMarked
public class NtbWebUiCoordinator implements AccessibilityState.Listener {
    private final ControlContainer mControlContainer;
    // The WebContents' ContentView. It draws nothing (the WebUI is drawn by the tab strip scene
    // layer) and only routes touch, mouse and hover events in the WebUI region to the WebContents.
    private final @Nullable ContentView mInputView;
    private final @Nullable EventForwarder mEventForwarder;
    private long mNativePtr;
    private int mWidthPx;
    private int mHeightPx;
    private boolean mStripInteractive = true;
    // The SystemClock.uptimeMillis() timestamp of the most recent input event forwarded from the
    // WebUI new tab button, consumed when the resulting tab is added to record
    // interaction-to-new-tab latency.
    private @Nullable Long mLastInteractionTimeMs;

    /**
     * Returns whether the tab strip should use the WebUI new tab button. Must be called after
     * native is initialized.
     */
    public static boolean isEnabled() {
        // Restricted to incognito-as-window so that each window hosts a single profile.
        return IncognitoUtils.shouldOpenIncognitoAsWindow()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ANDROID_NEW_TAB_BUTTON_TABSTRIP_WEB_UI);
    }

    /**
     * @param windowAndroid The {@link WindowAndroid} of the activity hosting the tab strip.
     * @param tabModel The {@link TabModel} of the window.
     * @param controlContainer The control container, which sits on top of the tab strip.
     * @param activityCreationUptimeMs The {@code SystemClock.uptimeMillis()} timestamp when the
     *     hosting activity was created, or 0 if unavailable.
     */
    public NtbWebUiCoordinator(
            WindowAndroid windowAndroid,
            TabModel tabModel,
            ControlContainer controlContainer,
            long activityCreationUptimeMs) {
        try (TraceEvent e = TraceEvent.scoped("NtbWebUiCoordinator.<init>")) {
            // The coordinator is created and owned by the activity's tab strip, so the activity
            // is alive at this point.
            Activity activity = assertNonNull(windowAndroid.getActivity().get());
            mControlContainer = controlContainer;

            // Native creates and owns the TabStripService and WebContents, and starts navigating
            // it to the WebUI. The navigation commits asynchronously, so the view and the window
            // attached below are in place before the page is created.
            int sessionId = assertNonNull(tabModel.getNativeSessionId());
            mNativePtr =
                    NtbWebUiCoordinatorJni.get().init(this, sessionId, activityCreationUptimeMs);
            if (mNativePtr == 0) {
                mInputView = null;
                mEventForwarder = null;
                return;
            }
            WebContents webContents = NtbWebUiCoordinatorJni.get().getWebContents(mNativePtr);
            mInputView = ContentView.createContentView(activity, webContents);
            webContents.setDelegates(
                    VersionInfo.getProductVersion(),
                    ViewAndroidDelegate.createBasicDelegate(mInputView),
                    mInputView,
                    windowAndroid,
                    WebContents.createDefaultInternalsHolder());
            mEventForwarder = webContents.getEventForwarder();

            // The view is only used for input; the content is drawn by the tab strip scene layer.
            mInputView.setAlpha(0f);
            // Accessibility is provided by the native strip for now. See
            // updateInputViewVisibility().
            mInputView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            mInputView.setFocusable(false);
            mInputView.setVisibility(View.GONE);
            // Adds the view on top of the tab strip.
            mControlContainer.setTabStripInputView(mInputView);
            AccessibilityState.addListener(this);
        }
    }

    /** Returns the pointer to the native TabStripNtbWebUiHost, or 0 if disabled or destroyed. */
    public long getNativePtr() {
        return mNativePtr;
    }

    @CalledByNative
    @VisibleForTesting
    void onInputInteraction(long eventTimeUptimeMs) {
        mLastInteractionTimeMs = eventTimeUptimeMs;
    }

    @Nullable Long takeLastInteractionTimeMs() {
        Long time = mLastInteractionTimeMs;
        mLastInteractionTimeMs = null;
        return time;
    }

    /**
     * Updates the bounds of the WebUI new tab button (and its input view) within the parent view.
     *
     * @param xDp The x position relative to the parent view, in dp.
     * @param yDp The y position relative to the parent view, in dp.
     * @param widthDp The width, in dp.
     * @param heightDp The height, in dp.
     */
    public void updateBounds(float xDp, float yDp, float widthDp, float heightDp) {
        if (mNativePtr == 0) return;
        assert mInputView != null && mEventForwarder != null;
        DisplayMetrics metrics = mInputView.getResources().getDisplayMetrics();
        int widthPx = ViewUtils.dpToPx(metrics, widthDp);
        int heightPx = ViewUtils.dpToPx(metrics, heightDp);
        int xPx = ViewUtils.dpToPx(metrics, xDp);
        int yPx = ViewUtils.dpToPx(metrics, yDp);
        mInputView.setTranslationX(xPx);
        mInputView.setTranslationY(yPx);
        // The parent view is aligned with the compositor surface. Touch sequences transferred to
        // Viz are in compositor surface coordinates, so Viz needs the input view's origin.
        mEventForwarder.setContainerViewOriginInCompositorSurface(xPx, yPx);
        if (widthPx != mWidthPx || heightPx != mHeightPx) {
            mWidthPx = widthPx;
            mHeightPx = heightPx;
            ViewGroup.LayoutParams params = mInputView.getLayoutParams();
            params.width = widthPx;
            params.height = heightPx;
            mInputView.setLayoutParams(params);
            if (widthPx > 0 && heightPx > 0) {
                NtbWebUiCoordinatorJni.get().resize(mNativePtr, widthPx, heightPx);
            }
        }
        updateInputViewVisibility();
    }

    /**
     * Sets whether the tab strip currently accepts input. This mirrors the native strip's own event
     * area: while the strip is hidden or animating its height (e.g. the toolbar sliding up into the
     * strip area), the input view must not intercept events meant for other UI.
     *
     * @param interactive Whether the tab strip is visible and accepts input.
     */
    public void setStripInteractive(boolean interactive) {
        if (mStripInteractive == interactive) return;
        mStripInteractive = interactive;
        updateInputViewVisibility();
    }

    /** Destroys the WebUI and removes the input view. */
    public void destroy() {
        if (mNativePtr == 0) return;
        mControlContainer.setTabStripInputView(null);
        NtbWebUiCoordinatorJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    // AccessibilityState.Listener:
    @Override
    public void onAccessibilityStateChanged(
            AccessibilityState.State oldAccessibilityState,
            AccessibilityState.State newAccessibilityState) {
        updateInputViewVisibility();
    }

    private void updateInputViewVisibility() {
        if (mNativePtr == 0) return;
        assert mInputView != null;
        // Accessibility for the new tab button is currently provided by the native strip, not by
        // the WebUI: StripLayoutHelper still exposes the (undrawn) native new tab button as a
        // VirtualView through CompositorViewHolder's accessibility node provider. TalkBack's
        // explore-by-touch is driven by hover events, which would otherwise be swallowed by this
        // input view since it sits above the CompositorViewHolder. Hide the input view while touch
        // exploration is enabled so that those events reach the native VirtualView. The WebUI keeps
        // rendering the button through the scene layer either way.
        // TODO(crbug.com/565326385): Expose the WebUI's own accessibility tree and keyboard focus.
        boolean showInputView =
                mStripInteractive
                        && mWidthPx > 0
                        && mHeightPx > 0
                        && !AccessibilityState.isTouchExplorationEnabled();
        mInputView.setVisibility(showInputView ? View.VISIBLE : View.GONE);
    }

    @NativeMethods
    interface Natives {
        long init(NtbWebUiCoordinator caller, int sessionId, long activityCreationUptimeMs);

        @JniType("content::WebContents*")
        WebContents getWebContents(long hostPtr);

        void resize(long hostPtr, int widthPx, int heightPx);

        void destroy(long hostPtr);
    }
}
