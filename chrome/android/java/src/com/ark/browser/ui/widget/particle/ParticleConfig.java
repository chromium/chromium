package com.ark.browser.ui.widget.particle;

/**
 * Created by: var_rain.
 * Created date: 2018/10/21.
 * Description: 粒子配置
 */
public class ParticleConfig {

    /* 粒子初始位置范围的宽度 */
    private int width;
    /* 粒子初始位置范围的高度 */
    private int height;
    /* 粒子的最小半径 */
    private int minRadius;
    /* 粒子的最大半径 */
    private int maxRadius;
    /* 粒子的X轴最小弹性可活动范围(默认-width/4) */
    private int minRangeX;
    /* 粒子的X轴最大弹性可活动范围(默认width/4) */
    private int maxRangeX;
    /* 粒子的Y轴最小弹性可活动范围(默认-height/4) */
    private int minRangeY;
    /* 粒子的Y轴最大弹性可活动范围(默认height/4) */
    private int maxRangeY;
    /* 跑完X轴的最小时间(单位:毫秒,默认4000) */
    private int minSpeedX;
    /* 跑完X轴的最大时间(单位:毫秒,默认10000) */
    private int maxSpeedX;
    /* 跑完Y轴的最小时间(单位:毫秒,默认4000) */
    private int minSpeedY;
    /* 跑完Y轴的最大时间(单位:毫秒,默认10000) */
    private int maxSpeedY;

    public ParticleConfig() {
        this.minSpeedX = 4000;
        this.minSpeedY = 4000;
        this.maxSpeedX = 10000;
        this.maxSpeedY = 10000;
    }

    public int getWidth() {
        return width;
    }

    public void setWidth(int width) {
        this.minRangeX = -width / 4;
        this.maxRangeX = width / 4;
        this.width = width;
    }

    public int getHeight() {
        return height;
    }

    public void setHeight(int height) {
        this.minRangeY = -height / 4;
        this.maxRangeY = height / 4;
        this.height = height;
    }

    public int getMinRadius() {
        return minRadius;
    }

    public void setMinRadius(int minRadius) {
        this.minRadius = minRadius;
    }

    public int getMaxRadius() {
        return maxRadius;
    }

    public void setMaxRadius(int maxRadius) {
        this.maxRadius = maxRadius;
    }

    public int getMinRangeX() {
        return minRangeX;
    }

    public void setMinRangeX(int minRangeX) {
        this.minRangeX = minRangeX;
    }

    public int getMaxRangeX() {
        return maxRangeX;
    }

    public void setMaxRangeX(int maxRangeX) {
        this.maxRangeX = maxRangeX;
    }

    public int getMinRangeY() {
        return minRangeY;
    }

    public void setMinRangeY(int minRangeY) {
        this.minRangeY = minRangeY;
    }

    public int getMaxRangeY() {
        return maxRangeY;
    }

    public void setMaxRangeY(int maxRangeY) {
        this.maxRangeY = maxRangeY;
    }

    public int getMinSpeedX() {
        return minSpeedX;
    }

    public void setMinSpeedX(int minSpeedX) {
        this.minSpeedX = minSpeedX;
    }

    public int getMaxSpeedX() {
        return maxSpeedX;
    }

    public void setMaxSpeedX(int maxSpeedX) {
        this.maxSpeedX = maxSpeedX;
    }

    public int getMinSpeedY() {
        return minSpeedY;
    }

    public void setMinSpeedY(int minSpeedY) {
        this.minSpeedY = minSpeedY;
    }

    public int getMaxSpeedY() {
        return maxSpeedY;
    }

    public void setMaxSpeedY(int maxSpeedY) {
        this.maxSpeedY = maxSpeedY;
    }
}
