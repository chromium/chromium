// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.UiUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The presenter that displays a single tab modal dialog.
 */
public class TabModalPresenter
        extends ModalDialogManager.Presenter implements ChromeFullscreenManager.FullscreenListener {
    private static final int ENTER_EXIT_ANIMATION_DURATION_MS = 200;

    /** The activity displaying the dialogs. */
    private final ChromeActivity mChromeActivity;

    /** The active tab of which the dialog will be shown on top. */
    private Tab mActiveTab;

    /** The parent view that contains the dialog container. */
    private ViewGroup mContainerParent;

    /** The container view that a dialog to be shown will be attached to. */
    private ViewGroup mDialogContainer;

    private ModalDialogView mDialogView;

    /** The model change processor that binds properties for the dialog view. */
    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    /** Whether the dialog container is brought to the front in its parent. */
    private boolean mContainerIsAtFront;

    /**
     * Whether an enter animation on the dialog container should run when
     * {@link #onBrowserControlsFullyVisible} is called.
     */
    private boolean mRunEnterAnimationOnCallback;

    /** Whether the action bar on selected text is temporarily cleared for showing dialogs. */
    private boolean mDidClearTextControls;

    /**
     * The sibling view of the dialog container drawn next in its parent when it should be behind
     * browser controls. If BottomSheet is opened or UrlBar is focused, the dialog container should
     * be behind the browser controls and the URL suggestions.
     */
    private View mDefaultNextSiblingView;

    /** Enter and exit animation duration that can be overwritten in tests. */
    private int mEnterExitAnimationDurationMs;

    private final ChromeFullscreenManager mChromeFullscreenManager;
    private int mBottomControlsHeight;
    private boolean mShouldUpdateContainerLayoutParams;

    private class ViewBinder extends ModalDialogViewBinder {
        @Override
        public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
            if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
                assert mDialogContainer != null;
                if (model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE)) {
                    mDialogContainer.setOnClickListener((v) -> {
                        dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                    });
                } else {
                    mDialogContainer.setOnClickListener(null);
                }
            } else {
                super.bind(model, view, propertyKey);
            }
        }
    }

    /**
     * Constructor for initializing dialog container.
     * @param chromeActivity The activity displaying the dialogs.
     */
    public TabModalPresenter(ChromeActivity chromeActivity) {
        mChromeActivity = chromeActivity;
        mEnterExitAnimationDurationMs = ENTER_EXIT_ANIMATION_DURATION_MS;
        mChromeFullscreenManager = mChromeActivity.getFullscreenManager();
        mChromeFullscreenManager.addListener(this);
    }

    public void destroy() {
        mChromeFullscreenManager.removeListener(this);
    }

    // ModalDialogManager.Presenter implementation.

    @Override
    protected void addDialogView(PropertyModel model) {
        if (mDialogContainer == null) initDialogContainer();
        updateContainerLayoutParams();
        int style = model.get(ModalDialogProperties.PRIMARY_BUTTON_FILLED)
                ? R.style.Theme_Chromium_ModalDialog_FilledPrimaryButton
                : R.style.Theme_Chromium_ModalDialog_TextPrimaryButton;
        mDialogView = (ModalDialogView) LayoutInflater
                              .from(new ContextThemeWrapper(mChromeActivity, style))
                              .inflate(R.layout.modal_dialog_view, null);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, mDialogView, new ViewBinder());

        setBrowserControlsAccess(true);
        // Don't show the dialog container before browser controls are guaranteed fully visible.
        if (mChromeFullscreenManager.areBrowserControlsFullyVisible()) {
            runEnterAnimation(mDialogView);
        } else {
            mRunEnterAnimationOnCallback = true;
        }
        mChromeActivity.addViewObscuringAllTabs(mDialogContainer);
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        setBrowserControlsAccess(false);
        // Don't run exit animation if enter animation has not yet started.
        if (mRunEnterAnimationOnCallback) {
            mRunEnterAnimationOnCallback = false;
        } else {
            // Clear focus so that keyboard can hide accordingly while entering tab switcher.
            mDialogView.clearFocus();
            runExitAnimation(mDialogView);
        }
        mChromeActivity.removeViewObscuringAllTabs(mDialogContainer);

        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        mDialogView = null;
    }

    // ChromeFullscreenManager.FullscreenListener implementation.

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {
        if (getDialogModel() == null || !mRunEnterAnimationOnCallback
                || !mChromeFullscreenManager.areBrowserControlsFullyVisible()) {
            return;
        }
        mRunEnterAnimationOnCallback = false;
        runEnterAnimation(mDialogView);
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        mBottomControlsHeight = bottomControlsHeight;
        mShouldUpdateContainerLayoutParams = true;
    }

    /**
     * Change view hierarchy for the dialog container to be either the front most or beneath the
     * toolbar.
     * @param toFront Whether the dialog container should be brought to the front.
     */
    void updateContainerHierarchy(boolean toFront) {
        if (toFront) {
            mDialogView.announceForAccessibility(getContentDescription(getDialogModel()));
            mDialogView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            mDialogView.requestFocus();
        } else {
            mDialogView.clearFocus();
            mDialogView.setImportantForAccessibility(
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }

        if (toFront == mContainerIsAtFront) return;
        mContainerIsAtFront = toFront;
        if (toFront) {
            mDialogContainer.bringToFront();
        } else {
            UiUtils.removeViewFromParent(mDialogContainer);
            UiUtils.insertBefore(mContainerParent, mDialogContainer, mDefaultNextSiblingView);
        }
    }

    // Calculate the top margin of the dialog container and the dialog scrim
    // so that the scrim doesn't overlap the toolbar.
    public static int getContainerTopMargin(Resources resources, int containerHeightResource) {
        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        int containerVerticalMargin = -scrimVerticalMargin;
        if (containerHeightResource != ChromeActivity.NO_CONTROL_CONTAINER) {
            containerVerticalMargin += resources.getDimensionPixelSize(containerHeightResource);
        }
        return containerVerticalMargin;
    }

    // Calculate the bottom margin of the dialog container.
    public static int getContainerBottomMargin(ChromeFullscreenManager manager) {
        return manager.getBottomControlsHeight();
    }

    /**
     * Inflate the dialog container in the dialog container view stub.
     */
    private void initDialogContainer() {
        ViewStub dialogContainerStub =
                mChromeActivity.findViewById(R.id.tab_modal_dialog_container_stub);
        dialogContainerStub.setLayoutResource(R.layout.modal_dialog_container);

        mDialogContainer = (ViewGroup) dialogContainerStub.inflate();
        mDialogContainer.setVisibility(View.GONE);

        // Make sure clicks are not consumed by content beneath the container view.
        mDialogContainer.setClickable(true);

        mContainerParent = (ViewGroup) mDialogContainer.getParent();
        // The default sibling view is the next view of the dialog container stub in main.xml and
        // should not be removed from its parent.
        mDefaultNextSiblingView =
                mChromeActivity.findViewById(R.id.tab_modal_dialog_container_sibling_view);
        assert mDefaultNextSiblingView != null;

        Resources resources = mChromeActivity.getResources();

        MarginLayoutParams params = (MarginLayoutParams) mDialogContainer.getLayoutParams();
        params.width = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.height = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.topMargin = getContainerTopMargin(
                resources, mChromeActivity.getControlContainerHeightResource());
        params.bottomMargin = getContainerBottomMargin(mChromeActivity.getFullscreenManager());
        mDialogContainer.setLayoutParams(params);

        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        View scrimView = mDialogContainer.findViewById(R.id.scrim);
        params = (MarginLayoutParams) scrimView.getLayoutParams();
        params.width = MarginLayoutParams.MATCH_PARENT;
        params.height = MarginLayoutParams.MATCH_PARENT;
        params.topMargin = scrimVerticalMargin;
        scrimView.setLayoutParams(params);
    }

    private void updateContainerLayoutParams() {
        if (!mShouldUpdateContainerLayoutParams) return;
        MarginLayoutParams params = (MarginLayoutParams) mDialogContainer.getLayoutParams();
        params.bottomMargin = mBottomControlsHeight;
        mDialogContainer.setLayoutParams(params);
        mShouldUpdateContainerLayoutParams = false;
    }

    /**
     * Set whether the browser controls access should be restricted. If true, dialogs are expected
     * to be showing and overflow menu would be disabled.
     * @param restricted Whether the browser controls access should be restricted.
     */
    private void setBrowserControlsAccess(boolean restricted) {
        if (mChromeActivity.getToolbarManager() == null) return;

        View menuButton = mChromeActivity.getToolbarManager().getMenuButtonView();

        if (restricted) {
            mActiveTab = mChromeActivity.getActivityTab();
            assert mActiveTab
                    != null : "Tab modal dialogs should be shown on top of an active tab.";

            // Hide contextual search panel so that bottom toolbar will not be
            // obscured and back press is not overridden.
            ContextualSearchManager contextualSearchManager =
                    mChromeActivity.getContextualSearchManager();
            if (contextualSearchManager != null) {
                contextualSearchManager.hideContextualSearch(
                        OverlayPanel.StateChangeReason.UNKNOWN);
            }

            // Dismiss the action bar that obscures the dialogs but preserve the text selection.
            WebContents webContents = mActiveTab.getWebContents();
            if (webContents != null) {
                SelectionPopupController controller =
                        SelectionPopupController.fromWebContents(webContents);
                controller.setPreserveSelectionOnNextLossOfFocus(true);
                mActiveTab.getContentView().clearFocus();
                controller.updateTextSelectionUI(false);
                mDidClearTextControls = true;
            }

            // Force toolbar to show and disable overflow menu.
            onTabModalDialogStateChanged(true);

            mChromeActivity.getToolbarManager().setUrlBarFocus(
                    false, LocationBar.OmniboxFocusReason.UNFOCUS);

            menuButton.setEnabled(false);
        } else {
            // Show the action bar back if it was dismissed when the dialogs were showing.
            if (mDidClearTextControls) {
                mDidClearTextControls = false;
                WebContents webContents = mActiveTab.getWebContents();
                if (webContents != null) {
                    SelectionPopupController.fromWebContents(webContents)
                            .updateTextSelectionUI(true);
                }
            }

            onTabModalDialogStateChanged(false);
            menuButton.setEnabled(true);
            mActiveTab = null;
        }
    }

    public static boolean isDialogShowing(Tab tab) {
        return TabAttributes.from(tab).get(TabAttributeKeys.MODAL_DIALOG_SHOWING, false);
    }

    private void onTabModalDialogStateChanged(boolean isShowing) {
        TabAttributes.from(mActiveTab).set(TabAttributeKeys.MODAL_DIALOG_SHOWING, isShowing);

        // Make sure to exit fullscreen mode before showing the tab modal dialog view.
        if (isShowing) mActiveTab.exitFullscreenMode();

        // Also need to update browser control state after dismissal to refresh the constraints.
        if (isShowing && areRendererInputEventsIgnored()) {
            mChromeFullscreenManager.showAndroidControls(true);
        } else {
            TabBrowserControlsState.update(mActiveTab, BrowserControlsState.SHOWN,
                    !mChromeFullscreenManager.offsetOverridden());
        }
    }

    private boolean areRendererInputEventsIgnored() {
        return mActiveTab.getWebContents().getMainFrame().areInputEventsIgnored();
    }

    /**
     * Helper method to run fade-in animation when the specified dialog view is shown.
     * @param dialogView The dialog view to be shown.
     */
    private void runEnterAnimation(View dialogView) {
        mDialogContainer.animate().cancel();
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        dialogView.setBackgroundResource(R.drawable.popup_bg_tinted);
        mDialogContainer.addView(dialogView, params);
        mDialogContainer.setAlpha(0f);
        mDialogContainer.setVisibility(View.VISIBLE);
        mDialogContainer.animate()
                .setDuration(mEnterExitAnimationDurationMs)
                .alpha(1f)
                .setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateContainerHierarchy(true);
                    }
                })
                .start();
    }

    /**
     * Helper method to run fade-out animation when the specified dialog view is dismissed.
     * @param dialogView The dismissed dialog view.
     */
    private void runExitAnimation(View dialogView) {
        mDialogContainer.animate().cancel();
        mDialogContainer.animate()
                .setDuration(mEnterExitAnimationDurationMs)
                .alpha(0f)
                .setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mDialogContainer.setVisibility(View.GONE);
                        mDialogContainer.removeView(dialogView);
                    }
                })
                .start();
    }

    @VisibleForTesting
    View getDialogContainerForTest() {
        return mDialogContainer;
    }

    @VisibleForTesting
    ViewGroup getContainerParentForTest() {
        return mContainerParent;
    }

    @VisibleForTesting
    void disableAnimationForTest() {
        mEnterExitAnimationDurationMs = 0;
    }
}
