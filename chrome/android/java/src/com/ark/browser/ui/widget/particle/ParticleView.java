package com.ark.browser.ui.widget.particle;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * Created by: var_rain.
 * Created date: 2018/10/21.
 * Description: 粒子效果
 */
public class ParticleView extends View {

    /* 例子数量 */
    private int count = 20;
    /* 透明度 */
    private int alpha = 60;
    /* 最小半径 */
    private int minSize = 30;
    /* 最大半径 */
    private int maxSize = 50;
    /* 颜色 */
    private int color;
    /* 背景 */
    private Bitmap background;
    /* 背景 */
    private BitmapDrawable drawable;
    /* 画笔 */
    private Paint paint;
    /* 宽度 */
    private int width;
    /* 高度 */
    private int height;
    /* 背景范围 */
    private Rect rect;
    /* 粒子配置 */
    private ParticleConfig config;
    /* 粒子数组 */
    private Particle[] particles;

    public ParticleView(Context context) {
        super(context);
    }

    public ParticleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        TypedArray array = context.obtainStyledAttributes(attrs, R.styleable.ParticleView);
        count = array.getInt(R.styleable.ParticleView_pv_count, 18);
        alpha = array.getInt(R.styleable.ParticleView_pv_alpha, 60);
        minSize = array.getInt(R.styleable.ParticleView_pv_min_size, 30);
        maxSize = array.getInt(R.styleable.ParticleView_pv_max_size, 50);
        color = array.getColor(R.styleable.ParticleView_pv_color, 0xffffff);
        drawable = (BitmapDrawable) array.getDrawable(R.styleable.ParticleView_pv_background);
        array.recycle();
        init();
    }

    public ParticleView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    /**
     * 初始化
     */
    private void init() {
        paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(color);
        if (drawable != null) {
            background = drawable.getBitmap();
        }
        // 不想开线程,属性动画凑合着用吧
        ValueAnimator animator = ValueAnimator.ofInt(0, 60);
        animator.setRepeatMode(ValueAnimator.RESTART);
        animator.setRepeatCount(-1);
        animator.setDuration(1000);
        animator.addUpdateListener(animation -> invalidate());
        animator.start();
    }

    @Override
    public void draw(Canvas canvas) {
        width = getWidth();
        height = getHeight();
        initRect();
        // 因为在View绘制的时候才能获取到View的宽高,所以在这里初始化粒子的配置,除非....你自己手动测量
        initParticleConfig();
        initParticles();
        paint.setAlpha(255);
        if (background != null) {
            canvas.drawBitmap(background, null, rect, paint);
        }
        paint.setAlpha(alpha);
        for (Particle particle : particles) {
            canvas.drawCircle(particle.getX(), particle.getY(), particle.getR(), paint);
        }
        super.draw(canvas);
    }

    /**
     * 初始化背景范围
     */
    private void initRect() {
        if (rect == null) {
            rect = new Rect(0, 0, width, height);
        }
    }

    /**
     * 初始化粒子配置
     */
    private void initParticleConfig() {
        if (config == null) {
            config = new ParticleConfig();
            config.setWidth(width);
            config.setHeight(height);
            config.setMaxRadius(maxSize);
            config.setMinRadius(minSize);
        }
    }

    /**
     * 初始化粒子
     */
    private void initParticles() {
        if (particles == null) {
            particles = new Particle[count];
            for (int i = 0; i < particles.length; i++) {
                particles[i] = new Particle(config);
            }
        }
    }
}
