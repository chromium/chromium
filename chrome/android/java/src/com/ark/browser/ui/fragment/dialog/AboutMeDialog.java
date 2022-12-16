package com.ark.browser.ui.fragment.dialog;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Bundle;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.ScriptIntrinsicBlur;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.ark.browser.utils.ThreadPool;
import com.zpj.fragmentation.dialog.animator.AbsDialogAnimator;
import com.zpj.fragmentation.dialog.DialogAnimator;
import com.zpj.fragmentation.dialog.impl.FullScreenDialogFragment;
import com.zpj.utils.AnimatorUtils;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.toolbar.ZToolBar;

import org.chromium.chrome.R;

public class AboutMeDialog extends FullScreenDialogFragment implements OnCommonItemClickListener {

    private ImageView ivBlur;
    private ImageView ivIcon;

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_about_me;
    }

    @Override
    protected DialogAnimator onCreateDialogAnimator(ViewGroup contentView) {
        return new DelayDialogAnimation(contentView);
    }

    @Override
    protected boolean enableSwipeBack() {
        return true;
    }

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        lightStatusBar();
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        ZToolBar toolBar = findViewById(R.id.tool_bar);
        toolBar.getLeftImageButton().setOnClickListener(v -> dismiss());
        ivIcon = findViewById(R.id.iv_icon);
        ivBlur = findViewById(R.id.iv_blur);


        initBackground();

        ivIcon.setImageBitmap(getCircleBitmap(BitmapFactory.decodeResource(getResources(), R.drawable.logo_author)));


        CommonSettingItem githubItem = findViewById(R.id.item_github);
        githubItem.setOnItemClickListener(this);

        CommonSettingItem sjlyItem = findViewById(R.id.item_sjly);
        sjlyItem.setOnItemClickListener(this);

        CommonSettingItem emailItem = findViewById(R.id.item_email);
        emailItem.setOnItemClickListener(this);
    }

    @Override
    public void onItemClick(CommonSettingItem item) {
        if (item.getId() == R.id.item_github || item.getId() == R.id.item_sjly) {
            Uri uri = Uri.parse(item.getInfoText());
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            startActivity(intent);
        } else if (item.getId() == R.id.item_email) {
            Uri uri = Uri.parse("mailto:" + item.getInfoText());
            Intent intent = new Intent(Intent.ACTION_SENDTO, uri);
            context.startActivity(Intent.createChooser(intent, "请选择邮件应用"));
        }
    }

    public static Bitmap getCircleBitmap(Bitmap bitmap) {
        if (bitmap == null) {
            return null;
        }
        try {
            Bitmap circleBitmap = Bitmap.createBitmap(bitmap.getWidth(),
                    bitmap.getHeight(), Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(circleBitmap);
            final Paint paint = new Paint();
            final Rect rect = new Rect(0, 0, bitmap.getWidth(),
                    bitmap.getHeight());
            final RectF rectF = new RectF(new Rect(0, 0, bitmap.getWidth(),
                    bitmap.getHeight()));
            float roundPx = 0.0f;
            roundPx = bitmap.getWidth();
            paint.setAntiAlias(true);
            canvas.drawARGB(0, 0, 0, 0);
            paint.setColor(Color.WHITE);
            canvas.drawRoundRect(rectF, roundPx, roundPx, paint);
            paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            final Rect src = new Rect(0, 0, bitmap.getWidth(),
                    bitmap.getHeight());
            canvas.drawBitmap(bitmap, src, rect, paint);
            return circleBitmap;
        } catch (Exception e) {
            return bitmap;
        }
    }

    public void initBackground() {
        ThreadPool.execute(() -> {

            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.logo_author);

            RenderScript rs = RenderScript.create(context);
            final BitmapFactory.Options options = new BitmapFactory.Options();
            options.inSampleSize = 3;

            final Allocation input = Allocation.createFromBitmap(rs, bitmap);
            final Allocation output = Allocation.createTyped(rs, input.getType());
            final ScriptIntrinsicBlur script = ScriptIntrinsicBlur.create(rs, Element.U8_4(rs));
            script.setRadius(12f);
            script.setInput(input);
            script.forEach(output);
            output.copyTo(bitmap);


            post(() -> {
                ivBlur.setImageBitmap(bitmap);
                ivBlur.setAlpha(0F);
                AnimatorUtils.changeViewAlpha(ivBlur, 0, 1, getShowAnimDuration());
                AnimatorUtils.changeViewSize(ivBlur, 4, 1, 800);
            });
        });
    }


    private class DelayDialogAnimation extends AbsDialogAnimator<Animator, Animator> {

        public DelayDialogAnimation(View target) {
            super(target);
        }

        @Override
        public Animator onCreateShowAnimator() {
            CommonSettingItem githubItem = findViewById(R.id.item_github);

            CommonSettingItem sjlyItem = findViewById(R.id.item_sjly);

            CommonSettingItem emailItem = findViewById(R.id.item_email);
            return doDelayShowAnim(getShowAnimDuration(), 100, ivIcon, findViewById(R.id.iv_name),
                    findViewById(R.id.tv_sign), githubItem, sjlyItem, emailItem);
        }

        @Override
        public Animator onCreateDismissAnimator() {
            findViewById(R.id.tool_bar).animate().alpha(0f).setDuration(getDismissAnimDuration()).start();
            ivBlur.animate().alpha(0f).setDuration(getDismissAnimDuration()).start();
            return doDelayHideAnim(240, 20, findViewById(R.id.iv_icon), findViewById(R.id.iv_name),
                    findViewById(R.id.tv_sign), findViewById(R.id.item_github),
                    findViewById(R.id.item_sjly), findViewById(R.id.item_email));
        }

        private Animator doDelayShowAnim(long dur, long delay, final View... targets) {
            AnimatorSet animatorSet = new AnimatorSet();
            AnimatorSet.Builder builder = null;
            for (int i = 0; i < targets.length; i++) {
                final View target = targets[i];
                target.setAlpha(0);
                ObjectAnimator animatorY = ObjectAnimator.ofFloat(target, "translationY", 1000, 0);
                ObjectAnimator animatorA = ObjectAnimator.ofFloat(target, "alpha", 0, 1);
                animatorY.setDuration(dur);
                animatorA.setDuration((long) (dur * 0.618F));
                AnimatorSet animator = new AnimatorSet();
                animator.playTogether(animatorA, animatorY);
                animator.setInterpolator(new DecelerateInterpolator());
                animator.setStartDelay(delay * i);
                animator.addListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        for (View view : targets) {
                            view.setVisibility(View.VISIBLE);
                        }
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }
                });
                if (builder == null) {
                    builder = animatorSet.play(animator);
                } else {
                    builder.with(animator);
                }
            }
            return animatorSet;
        }

        private Animator doDelayHideAnim(long dur, long delay, final View... targets) {
            AnimatorSet animatorSet = new AnimatorSet();
            AnimatorSet.Builder builder = null;
            for (int i = 0; i < targets.length; i++) {
                final View target = targets[i];
                target.setAlpha(1f);
                ObjectAnimator animatorY = ObjectAnimator.ofFloat(target, "translationY", 0, 100);
                ObjectAnimator animatorA = ObjectAnimator.ofFloat(target, "alpha", 1, 0);
                animatorY.setDuration(dur);
                animatorA.setDuration((long) (dur * 0.618F));
                AnimatorSet animator = new AnimatorSet();
                animator.playTogether(animatorA, animatorY);
                animator.setInterpolator(new DecelerateInterpolator());
                animator.setStartDelay(delay * i);
                animator.addListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {

                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        for (View view : targets) {
                            view.setVisibility(View.INVISIBLE);
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                    }

                    @Override
                    public void onAnimationRepeat(Animator animation) {
                    }
                });
                if (builder == null) {
                    builder = animatorSet.play(animator);
                } else {
                    builder.with(animator);
                }
            }
            return animatorSet;
        }

    }

}

