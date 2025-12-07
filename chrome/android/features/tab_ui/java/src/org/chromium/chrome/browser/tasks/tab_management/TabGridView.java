// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.BASE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tab.Tab.MediaState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/** Holds the view for a selectable tab grid. */
@NullMarked
public class TabGridView extends SelectableItemViewBase<TabListEditorItemSelectionId> {
    private static final long RESTORE_ANIMATION_DURATION_MS = 50;
    private static final float ZOOM_IN_SCALE = 0.8f;

    private static @Nullable WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    @IntDef({
        QuickDeleteAnimationStatus.TAB_HIDE,
        QuickDeleteAnimationStatus.TAB_PREPARE,
        QuickDeleteAnimationStatus.TAB_RESTORE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface QuickDeleteAnimationStatus {
        int TAB_RESTORE = 0;
        int TAB_PREPARE = 1;
        int TAB_HIDE = 2;
        int NUM_ENTRIES = 3;
    }

    private TabCardHighlightHandler mTabCardHighlightHandler;
    private boolean mIsAnimating;
    private @TabActionButtonType int mTabActionButtonType;
    private @TabActionState int mTabActionState = TabActionState.UNSET;
    private @Nullable ObjectAnimator mQuickDeleteAnimation;
    private @Nullable QuickDeleteAnimationGradientDrawable mQuickDeleteAnimationDrawable;
    private ImageView mActionButton;
    private @Nullable ColorStateList mActionButtonTint;

    public TabGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButton = findViewById(R.id.action_button);
        View cardWrapper = findViewById(R.id.card_wrapper);
        assert cardWrapper != null;
        mTabCardHighlightHandler = new TabCardHighlightHandler(cardWrapper);
    }

    /**
     * Play the zoom-in and zoom-out animations for tab grid card.
     *
     * @param status The target animation status in {@link AnimationStatus}.
     */
    void scaleTabGridCardView(@AnimationStatus int status) {
        assert mTabActionState != TabActionState.UNSET;
        assert status < AnimationStatus.NUM_ENTRIES;

        final View backgroundView = fastFindViewById(R.id.background_view);
        final View contentView = fastFindViewById(R.id.content_view);
        boolean isZoomIn =
                status == AnimationStatus.SELECTED_CARD_ZOOM_IN
                        || status == AnimationStatus.HOVERED_CARD_ZOOM_IN;
        boolean isHovered =
                status == AnimationStatus.HOVERED_CARD_ZOOM_IN
                        || status == AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        boolean isRestore = status == AnimationStatus.CARD_RESTORE;
        long duration = isRestore ? RESTORE_ANIMATION_DURATION_MS : BASE_ANIMATION_DURATION_MS;
        float scale = isZoomIn ? ZOOM_IN_SCALE : 1f;
        View animateView = isHovered ? contentView : this;

        if (status == AnimationStatus.HOVERED_CARD_ZOOM_IN) {
            backgroundView.setVisibility(View.VISIBLE);
        }

        AnimatorSet scaleAnimator = new AnimatorSet();
        scaleAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (!isZoomIn) {
                            backgroundView.setVisibility(View.GONE);
                        }
                        mIsAnimating = false;
                    }
                });

        ObjectAnimator scaleX = ObjectAnimator.ofFloat(animateView, View.SCALE_X, scale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(animateView, View.SCALE_Y, scale);
        scaleX.setDuration(duration);
        scaleY.setDuration(duration);
        scaleAnimator.play(scaleX).with(scaleY);
        mIsAnimating = true;
        scaleAnimator.start();
    }

    void hideTabGridCardViewForQuickDelete(
            @QuickDeleteAnimationStatus int status, boolean isIncognito) {
        assert mTabActionState != TabActionState.UNSET;
        assert status < QuickDeleteAnimationStatus.NUM_ENTRIES;

        final ViewGroup contentView = (ViewGroup) fastFindViewById(R.id.content_view);
        if (contentView == null) return;

        if (status == QuickDeleteAnimationStatus.TAB_HIDE) {
            assert mQuickDeleteAnimation != null && mQuickDeleteAnimationDrawable != null;

            contentView.setForeground(mQuickDeleteAnimationDrawable);
            mQuickDeleteAnimation.start();
        } else if (status == QuickDeleteAnimationStatus.TAB_PREPARE) {
            Drawable originalForeground = contentView.getForeground();
            int tabHeight = contentView.getHeight();
            mQuickDeleteAnimationDrawable =
                    QuickDeleteAnimationGradientDrawable.createQuickDeleteFadeAnimationDrawable(
                            getContext(), tabHeight, isIncognito);
            mQuickDeleteAnimation = mQuickDeleteAnimationDrawable.createFadeAnimator(tabHeight);

            mQuickDeleteAnimation.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            contentView.setVisibility(GONE);
                            contentView.setForeground(originalForeground);
                        }
                    });
        } else if (status == QuickDeleteAnimationStatus.TAB_RESTORE) {
            // Reset to original values to allow the tab to be recycled correctly.
            mQuickDeleteAnimation = null;
            mQuickDeleteAnimationDrawable = null;
            contentView.setVisibility(VISIBLE);
        }
    }

    void setTabActionButtonDrawable(@TabActionButtonType int type) {
        assert mTabActionState != TabActionState.UNSET;

        if (mTabActionState != TabActionState.CLOSABLE) return;

        mTabActionButtonType = type;
        setTabActionButtonDrawable();

        applyActionButtonTint();
    }

    void setTabActionButtonTint(ColorStateList actionButtonTint) {
        mActionButtonTint = actionButtonTint;
        setTabActionButtonDrawable();
    }

    void setTabActionState(@TabActionState int tabActionState) {
        if (mTabActionState == tabActionState) return;

        mTabActionState = tabActionState;
        int accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_YES;
        if (mTabActionState == TabActionState.CLOSABLE) {
            setTabActionButtonDrawable();
        } else if (mTabActionState == TabActionState.SELECTABLE) {
            accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_NO;
            setTabActionButtonSelectionDrawable();
        }

        mActionButton.setImportantForAccessibility(accessibilityMode);
    }

    void setIsHighlighted(@TabCardHighlightState int highlightState, boolean isIncognito) {
        mTabCardHighlightHandler.maybeAnimateForHighlightState(highlightState, isIncognito);
    }

    void setMediaIndicator(@MediaState int mediaState) {
        // TODO(crbug.com/430072416): Add other media indicators.
        TextView tabTitle = findViewById(R.id.tab_title);
        ImageView tabMediaIndicator = findViewById(R.id.media_indicator_icon);
        tabMediaIndicator.setImageResource(TabUtils.getMediaIndicatorDrawable(mediaState));
        ConstraintLayout.LayoutParams titleParams =
                (ConstraintLayout.LayoutParams) tabTitle.getLayoutParams();

        int mediaIndicatorVisibility = View.GONE;
        switch (mediaState) {
            case MediaState.AUDIBLE:
            case MediaState.MUTED:
            case MediaState.RECORDING:
            case MediaState.SHARING:
                titleParams.endToEnd = R.id.media_indicator_icon;
                mediaIndicatorVisibility = View.VISIBLE;
                break;
            case MediaState.NONE:
                titleParams.endToEnd = R.id.card_view;
                break;
            default:
                assert false : "Invalid media state";
                break;
        }

        tabTitle.setLayoutParams(titleParams);
        tabMediaIndicator.setVisibility(mediaIndicatorVisibility);
    }

    void clearHighlight() {
        mTabCardHighlightHandler.clearHighlight();
    }

    private void setTabActionButtonCloseDrawable() {
        assert mTabActionState != TabActionState.UNSET;

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef =
                    new WeakReference<>(
                            Bitmap.createScaledBitmap(
                                    bitmap, closeButtonSize, closeButtonSize, true));
            bitmap.recycle();
        }
        mActionButton.setBackgroundResource(R.drawable.small_icon_background);
        mActionButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
        mActionButton.setFocusable(true);
    }

    private void setTabActionButtonPinDrawable() {
        assert mTabActionState != TabActionState.UNSET;

        mActionButton.setImageDrawable(
                ContextCompat.getDrawable(getContext(), R.drawable.ic_keep_24dp));
        mActionButton.setBackground(null);
        mActionButton.setFocusable(false);
    }

    private void setTabActionButtonOverflowDrawable() {
        mActionButton.setImageDrawable(
                ResourcesCompat.getDrawable(
                        getResources(), R.drawable.ic_more_vert_24dp, getContext().getTheme()));
        mActionButton.setFocusable(true);
    }

    private void applyActionButtonTint() {
        ImageViewCompat.setImageTintList(mActionButton, mActionButtonTint);
    }

    private void setTabActionButtonSelectionDrawable() {
        assert mTabActionState != TabActionState.UNSET;
        var resources = getResources();
        Drawable selectionListIcon =
                ResourcesCompat.getDrawable(
                        resources,
                        R.drawable.tab_grid_selection_list_icon,
                        getContext().getTheme());

        InsetDrawable drawable =
                new InsetDrawable(
                        selectionListIcon,
                        (int)
                                resources.getDimension(
                                        R.dimen.selection_tab_grid_toggle_button_inset));
        mActionButton.setBackground(drawable);
        mActionButton
                .getBackground()
                .setLevel(resources.getInteger(R.integer.list_item_level_default));
        mActionButton.setImageDrawable(
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.ic_check_googblue_20dp_animated));
        mActionButton.setFocusable(false);
    }

    private void setTabActionButtonDrawable() {
        if (mTabActionButtonType == TabActionButtonType.OVERFLOW) {
            setTabActionButtonOverflowDrawable();
        } else if (mTabActionButtonType == TabActionButtonType.PIN) {
            setTabActionButtonPinDrawable();
        } else {
            setTabActionButtonCloseDrawable();
        }

        applyActionButtonTint();
    }

    // SelectableItemViewBase implementation.
    @Override
    protected void handleNonSelectionClick() {}

    // TODO(crbug.com/339038201): Consider capturing click events and discarding them while not in
    // selection mode.

    // View implementation.

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        if (mTabActionState == TabActionState.SELECTABLE) {
            info.setCheckable(true);
            info.setChecked(isChecked());
        }
    }

    // Testing methods.

    boolean getIsAnimatingForTesting() {
        return mIsAnimating;
    }
}
