package com.ark.browser.ui.widget.particle;

import android.animation.ValueAnimator;

import java.util.Random;

/**
 * Created by: var_rain.
 * Created date: 2018/10/21.
 * Description: 粒子对象
 */
public class Particle {

    /* X坐标 */
    private int x;
    /* Y坐标 */
    private int y;
    /* 半径 */
    private int r;
    /* 属性配置 */
    private ParticleConfig config;
    /* 变换X值 */
    private int saveX;
    /* 变换Y值 */
    private int saveY;

    /**
     * 构造方法
     *
     * @param config 粒子属性配置
     */
    public Particle(ParticleConfig config) {
        this.config = config;
        this.x = random(0, config.getWidth());
        this.y = random(0, config.getHeight());
        this.r = random(config.getMinRadius(), config.getMaxRadius());
        initValueX();
        initValueY();
    }

    /**
     * 初始化X轴属性变换动画
     */
    private void initValueX() {
        ValueAnimator xValue = ValueAnimator.ofInt(config.getMinRangeX(), config.getMaxRangeX());
        xValue.setDuration(random(config.getMinSpeedX(), config.getMaxSpeedX()));
        xValue.setRepeatMode(ValueAnimator.REVERSE);
        xValue.setRepeatCount(-1);
        xValue.addUpdateListener(animation -> saveX = (int) animation.getAnimatedValue());
        xValue.start();
    }

    /**
     * 初始化Y周属性变化动画
     */
    private void initValueY() {
        ValueAnimator yValue = ValueAnimator.ofInt(config.getMinRangeY(), config.getMaxRangeY());
        yValue.setDuration(random(config.getMinSpeedY(), config.getMaxSpeedY()));
        yValue.setRepeatMode(ValueAnimator.REVERSE);
        yValue.setRepeatCount(-1);
        yValue.addUpdateListener(animation -> saveY = (int) animation.getAnimatedValue());
        yValue.start();
    }

    /**
     * 获取X坐标
     *
     * @return 返回int值
     */
    public int getX() {
        return x + saveX;
    }

    /**
     * 获取Y坐标
     *
     * @return 返回int值
     */
    public int getY() {
        return y + saveY;
    }

    /**
     * 获取半径
     *
     * @return 返回int值
     */
    public int getR() {
        return r;
    }

    /**
     * 随机取数
     *
     * @param min 最小值
     * @param max 最大值
     * @return 返回一个在最小值和最大值之间的数
     */
    private int random(int min, int max) {
        Random r = new Random();
        return r.nextInt(max) % (max - min + 1) + min;
    }
}
