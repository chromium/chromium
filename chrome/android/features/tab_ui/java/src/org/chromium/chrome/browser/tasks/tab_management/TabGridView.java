// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

// TODO(crbug.com/339038505): De-dupe logic in TabListView.
/** Holds the view for a selectable tab grid. */
public class TabGridView extends SelectableItemViewBase<Integer> {
    private static final long RESTORE_ANIMATION_DURATION_MS = 50;
    private static final long BASE_ANIMATION_DURATION_MS = 218;
    private static final float ZOOM_IN_SCALE = 0.8f;

    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    @IntDef({
        AnimationStatus.SELECTED_CARD_ZOOM_IN,
        AnimationStatus.SELECTED_CARD_ZOOM_OUT,
        AnimationStatus.HOVERED_CARD_ZOOM_IN,
        AnimationStatus.HOVERED_CARD_ZOOM_OUT,
        AnimationStatus.CARD_RESTORE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationStatus {
        int CARD_RESTORE = 0;
        int SELECTED_CARD_ZOOM_OUT = 1;
        int SELECTED_CARD_ZOOM_IN = 2;
        int HOVERED_CARD_ZOOM_OUT = 3;
        int HOVERED_CARD_ZOOM_IN = 4;
        int NUM_ENTRIES = 5;
    }

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

    private boolean mIsAnimating;
    private @TabActionState int mTabActionState = TabActionState.UNSET;
    private @Nullable ObjectAnimator mQuickDeleteAnimation;
    private @Nullable QuickDeleteAnimationGradientDrawable mQuickDeleteAnimationDrawable;
    private ImageView mActionButton;

    public TabGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButton = findViewById(R.id.action_button);
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

    void setTabActionButtonDrawable(boolean showOverflowButton) {
        assert mTabActionState != TabActionState.UNSET;

        if (mTabActionState != TabActionState.CLOSABLE) return;

        if (showOverflowButton) {
            mActionButton.setImageDrawable(
                    ResourcesCompat.getDrawable(
                            getResources(), R.drawable.ic_more_vert_24dp, getContext().getTheme()));
        } else {
            setTabActionButtonCloseDrawable();
        }
    }

    void setTabActionState(@TabActionState int tabActionState) {
        if (mTabActionState == tabActionState) return;

        mTabActionState = tabActionState;
        int accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_YES;
        if (mTabActionState == TabActionState.CLOSABLE) {
            setTabActionButtonCloseDrawable();
        } else if (mTabActionState == TabActionState.SELECTABLE) {
            accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_NO;
            setTabActionButtonSelectionDrawable();
        }

        mActionButton.setImportantForAccessibility(accessibilityMode);
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
        mActionButton.setBackground(null);
        mActionButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
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
    }

    // SelectableItemViewBase implementation.

    @Override
    protected void updateView(boolean animate) {}

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
