// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.chromium.chrome.browser.readaloud.player.VisibilityState.GONE;
import static org.chromium.chrome.browser.readaloud.player.VisibilityState.HIDING;
import static org.chromium.chrome.browser.readaloud.player.VisibilityState.SHOWING;
import static org.chromium.chrome.browser.readaloud.player.VisibilityState.VISIBLE;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.Colors;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Bottom sheet content for Read Aloud expanded player advanced options menu. */
class OptionsMenuSheetContent extends MenuSheetContent {
    private static final String TAG = "ReadAloudOptions";
    private static final long VOICE_MENU_TRANSITION_MS = 600;
    private final Context mContext;
    private final PropertyModel mModel;
    private InteractionHandler mHandler;

    // Contents
    private final FrameLayout mContainer;
    private final Menu mOptionsMenu;
    private final VoiceMenu mVoiceMenu;

    // Voice menu visibility and transition animations
    private @VisibilityState int mVoiceMenuState = GONE;
    private final AnimatorSet mVoiceMenuShowAnimation;
    private final ObjectAnimator mVoiceMenuIn;
    private final ObjectAnimator mOptionsMenuOut;
    private final AnimatorListenerAdapter mAnimationListener =
            new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    if (mVoiceMenuState == SHOWING) {
                        mVoiceMenuState = VISIBLE;
                    } else if (mVoiceMenuState == HIDING) {
                        mVoiceMenuState = GONE;
                        mVoiceMenu.getMenu().setVisibility(View.GONE);
                        if (mHandler != null) {
                            mHandler.onVoiceMenuClosed();
                        }
                    }
                }
            };

    @IntDef({Item.VOICE, Item.TRANSLATE, Item.HIGHLIGHT})
    @Retention(RetentionPolicy.SOURCE)
    @interface Item {
        int VOICE = 0;
        int TRANSLATE = 1;
        int HIGHLIGHT = 2;
    }

    public OptionsMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            PropertyModel model) {
        this(context, parent, bottomSheetController, model, LayoutInflater.from(context));
    }

    @VisibleForTesting
    OptionsMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            PropertyModel model,
            LayoutInflater layoutInflater) {
        super(parent, bottomSheetController);
        mContext = context;
        mModel = model;

        Resources res = mContext.getResources();

        // Set up options menu
        mOptionsMenu = (Menu) layoutInflater.inflate(R.layout.readaloud_menu, null);
        mOptionsMenu.addItem(
                Item.VOICE,
                R.drawable.voice_selection_24,
                res.getString(R.string.readaloud_voice_menu_title),
                /* header */ null,
                MenuItem.Action.EXPAND);
        mOptionsMenu.addItem(
                Item.HIGHLIGHT,
                R.drawable.format_ink_highlighter_24,
                res.getString(R.string.readaloud_highlight_toggle_name),
                /*header*/ null,
                MenuItem.Action.TOGGLE);
        mOptionsMenu.setItemClickHandler(this::onOptionsMenuClick);
        mOptionsMenu.addOnLayoutChangeListener(this::onOptionsMenuLayoutChange);
        mOptionsMenu.afterInflating(
                () -> {
                    // Views inside menu layout are only available after inflating.
                    mOptionsMenu.setTitle(R.string.readaloud_options_menu_title);
                    mOptionsMenu.setBackPressHandler(this::onBackPressed);
                });

        // Set up voice menu
        mVoiceMenu = new VoiceMenu(context, mModel, this::hideVoiceMenu);

        // Put both in content view
        mContainer = new FrameLayout(context);
        mContainer.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT));
        mContainer.addView(mOptionsMenu);
        mContainer.addView(mVoiceMenu.getMenu());
        Colors.setBottomSheetContentBackground(mContainer);

        // Create animations. Start and end values won't be correct until layout so they need to be
        // set later too.
        mVoiceMenuIn =
                ObjectAnimator.ofFloat(
                        mVoiceMenu.getMenu(),
                        View.TRANSLATION_X,
                        (float) mOptionsMenu.getWidth(),
                        0f);
        mOptionsMenuOut =
                ObjectAnimator.ofFloat(
                        mOptionsMenu, View.TRANSLATION_X, 0f, (float) -mOptionsMenu.getWidth());
        mVoiceMenuShowAnimation = new AnimatorSet();
        mVoiceMenuShowAnimation.play(mVoiceMenuIn).with(mOptionsMenuOut);
        mVoiceMenuShowAnimation.setDuration(VOICE_MENU_TRANSITION_MS);
        mVoiceMenuShowAnimation.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        mVoiceMenuShowAnimation.addListener(mAnimationListener);
    }

    public VoiceMenu getVoiceMenu() {
        return mVoiceMenu;
    }

    private void onOptionsMenuLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // Do nothing if options menu width is unchanged.
        if ((right - left) == (oldRight - oldLeft)) {
            return;
        }

        // Skip to the end of the animation if running, or just use animation to set X values if not
        // running.
        updateAnimationValues(mVoiceMenuState);

        // Update the views' X translation using animation end(), which does nothing if the
        // animation isn't started first.
        if (!mVoiceMenuShowAnimation.isRunning()) {
            mVoiceMenuShowAnimation.start();
        }
        mVoiceMenuShowAnimation.end();
    }

    void setInteractionHandler(InteractionHandler handler) {
        mHandler = handler;
        mOptionsMenu
                .getItem(Item.HIGHLIGHT)
                .setToggleHandler(
                        (value) -> {
                            handler.onHighlightingChange(value);
                        });
    }

    void setHighlightingSupported(boolean supported) {
        mOptionsMenu.getItem(Item.HIGHLIGHT).setItemEnabled(supported);
    }

    void setHighlightingEnabled(boolean enabled) {
        mOptionsMenu.getItem(Item.HIGHLIGHT).setValue(enabled);
    }

    @Override
    public void notifySheetClosed(BottomSheetContent closingContent) {
        super.notifySheetClosed(closingContent);
        // If sheet is closed with the voice menu open, hide it and skip to the end of the
        // animation.
        if (closingContent == this
                && (mVoiceMenuState == SHOWING || mVoiceMenuState == VISIBLE)
                && mHandler != null) {
            hideVoiceMenu();
            mVoiceMenuShowAnimation.end();
            mHandler.onVoiceMenuClosed();
        }
    }

    void onOrientationChange(int orientation) {
        mOptionsMenu.onOrientationChange(orientation);
        mVoiceMenu.getMenu().onOrientationChange(orientation);
    }

    // BottomSheetContent
    @Override
    public View getContentView() {
        return mContainer;
    }

    @Override
    public int getVerticalScrollOffset() {
        if (mVoiceMenuState == VISIBLE) {
            return mVoiceMenu.getMenu().getScrollView().getScrollY();
        }
        return mOptionsMenu.getScrollView().getScrollY();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // "Options menu"
        // Automatically appended: "Swipe down to close."
        return R.string.readaloud_options_menu_description;
    }

    @Override
    public void onBackPressed() {
        // Intercept back gesture if voice menu is open.
        if (mVoiceMenuState == VISIBLE) {
            hideVoiceMenu();
        } else {
            super.onBackPressed();
        }
    }

    private void onOptionsMenuClick(int itemId) {
        switch (itemId) {
            case Item.VOICE:
                showVoiceMenu();
                break;

            case Item.HIGHLIGHT:
                // Nothing to do here, MenuItem converts click into a toggle for which a
                // listener is registered elsewhere.
                break;
        }
    }

    private void showVoiceMenu() {
        if (mVoiceMenuState == SHOWING || mVoiceMenuState == VISIBLE) {
            return;
        }

        mVoiceMenu.getMenu().setVisibility(View.VISIBLE);
        runVoiceMenuAnimation(SHOWING);
    }

    private void hideVoiceMenu() {
        if (mVoiceMenuState == HIDING || mVoiceMenuState == GONE) {
            return;
        }

        runVoiceMenuAnimation(HIDING);
    }

    private void runVoiceMenuAnimation(@VisibilityState int newState) {
        mVoiceMenuShowAnimation.pause();
        long reversedPlayTimeMs =
                mVoiceMenuShowAnimation.getTotalDuration()
                        - mVoiceMenuShowAnimation.getCurrentPlayTime();
        mVoiceMenuShowAnimation.cancel();

        updateAnimationValues(newState);

        if ((newState == SHOWING && mVoiceMenuState == HIDING)
                || (newState == HIDING && mVoiceMenuState == SHOWING)) {
            mVoiceMenuShowAnimation.setCurrentPlayTime(reversedPlayTimeMs);
        }

        mVoiceMenuShowAnimation.start();
        mVoiceMenuState = newState;
    }

    private void updateAnimationValues(@VisibilityState int newState) {
        // Set start and end values for X translation.
        float optionsMenuWidth = (float) mOptionsMenu.getWidth();
        if (newState == SHOWING || newState == VISIBLE) {
            mVoiceMenuIn.setFloatValues(optionsMenuWidth, 0f);
            mOptionsMenuOut.setFloatValues(0f, -optionsMenuWidth);
        } else if (newState == HIDING || newState == GONE) {
            mVoiceMenuIn.setFloatValues(0f, optionsMenuWidth);
            mOptionsMenuOut.setFloatValues(-optionsMenuWidth, 0f);
        }
    }

    Menu getMenuForTesting() {
        return mOptionsMenu;
    }

    AnimatorSet getVoiceMenuShowAnimationForTesting() {
        return mVoiceMenuShowAnimation;
    }
}
