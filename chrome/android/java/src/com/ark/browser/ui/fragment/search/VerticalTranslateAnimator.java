package com.ark.browser.ui.fragment.search;

import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;

import com.zpj.fragmentation.dialog.animator.AbsDialogAnimator;
import com.zpj.fragmentation.dialog.enums.DialogAnimation;

import org.chromium.chrome.R;

/**
 * 平移动画，不带渐变
 */
public class VerticalTranslateAnimator extends AbsDialogAnimator<Animation, Animation> {


    public VerticalTranslateAnimator(View target, DialogAnimation dialogAnimation) {
        super(target, dialogAnimation);
    }

    @Override
    public void animateToShow() {
        Animation animator = onCreateShowAnimator();
        startAnimator(animator, mShowDuration, true);
    }

    @Override
    public void animateToDismiss() {
        Animation animator = onCreateDismissAnimator();
        startAnimator(animator, mDismissDuration, false);
    }

    @Override
    public Animation onCreateShowAnimator() {
        return AnimationUtils.loadAnimation(targetView.getContext(), R.anim.v_fragment_enter);
    }

    @Override
    public Animation onCreateDismissAnimator() {
        return AnimationUtils.loadAnimation(targetView.getContext(), R.anim.v_fragment_exit);
    }
}

