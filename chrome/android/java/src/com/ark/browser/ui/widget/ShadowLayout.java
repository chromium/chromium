package com.ark.browser.ui.widget;

/**
 * Created by leo
 * on 2019/7/9.
 * 阴影控件
 */

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;


public class ShadowLayout extends FrameLayout {

    private int mBackGroundColor;
    private int mBackGroundColorClicked;
    private int mShadowColor;
    private float mShadowLimit;
    private float mCornerRadius;
    private float mDx;
    private float mDy;
    private boolean leftShow;
    private boolean rightShow;
    private boolean topShow;
    private boolean bottomShow;
    private Paint shadowPaint;
    private Paint paint;

    private int leftPading;
    private int topPading;
    private int rightPading;
    private int bottomPading;
    //阴影布局子空间区域
    private RectF rectf = new RectF();

    //ShadowLayout的样式，是只需要pressed还是selected,还是2者都需要，默认支持2者
    private int selectorType = 3;
    private boolean isShowShadow = true;
    private boolean isSym;

    //增加各个圆角的属性
    private float mCornerRadius_leftTop;
    private float mCornerRadius_rightTop;
    private float mCornerRadius_leftBottom;
    private float mCornerRadius_rightBottom;

    public ShadowLayout(Context context) {
        this(context, null);
    }

    public ShadowLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }


    public ShadowLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initView(context, attrs);
    }

    //增加selector样式
    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);

        if (selectorType == 3 || selectorType == 2) {
            if (selected) {
                paint.setColor(mBackGroundColorClicked);
            } else {
                paint.setColor(mBackGroundColor);
            }
            postInvalidate();
            invalidate();
        }
    }


    //动态设置x轴偏移量
    public void setMDx(float mDx) {
        if (Math.abs(mDx) > mShadowLimit) {
            if (mDx > 0) {
                this.mDx = mShadowLimit;
            } else {
                this.mDx = -mShadowLimit;
            }
        } else {
            this.mDx = mDx;
        }
        setPading();
    }

    //动态设置y轴偏移量
    public void setMDy(float mDy) {
        if (Math.abs(mDy) > mShadowLimit) {
            if (mDy > 0) {
                this.mDy = mShadowLimit;
            } else {
                this.mDy = -mShadowLimit;
            }
        } else {
            this.mDy = mDy;
        }
        setPading();
    }


    public float getmCornerRadius() {
        return mCornerRadius;
    }

    //动态设置 圆角属性
    public void setmCornerRadius(int mCornerRadius) {
        this.mCornerRadius = mCornerRadius;
        if (getWidth() != 0 && getHeight() != 0) {
            setBackgroundCompat(getWidth(), getHeight());
        }
    }

    public float getmShadowLimit() {
        return mShadowLimit;
    }

    //动态设置阴影扩散区域
    public void setmShadowLimit(int mShadowLimit) {
        this.mShadowLimit = mShadowLimit;
        setPading();
    }

    //动态设置阴影颜色值
    public void setmShadowColor(int mShadowColor) {
        this.mShadowColor = mShadowColor;
        if (getWidth() != 0 && getHeight() != 0) {
            setBackgroundCompat(getWidth(), getHeight());
        }
    }


    public void setLeftShow(boolean leftShow) {
        this.leftShow = leftShow;
        setPading();
    }

    public void setRightShow(boolean rightShow) {
        this.rightShow = rightShow;
        setPading();
    }

    public void setTopShow(boolean topShow) {
        this.topShow = topShow;
        setPading();
    }

    public void setBottomShow(boolean bottomShow) {
        this.bottomShow = bottomShow;
        setPading();
    }

    @Override
    public void setBackgroundColor(int color) {
        this.mBackGroundColor = color;
        setSelected(isSelected());
        postInvalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (w > 0 && h > 0) {
            setBackgroundCompat(w, h);
        }
    }

    private void initView(Context context, AttributeSet attrs) {
        initAttributes(attrs);
        shadowPaint = new Paint();
        shadowPaint.setAntiAlias(true);
        shadowPaint.setStyle(Paint.Style.FILL);


        //矩形画笔
        paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(mBackGroundColor);

        setPading();
    }


    public void setPading() {
        //控件区域是否对称，默认是对称。不对称的话，那么控件区域随着阴影区域走
        if (isSym) {
            int xPadding = (int) (mShadowLimit + Math.abs(mDx));
            int yPadding = (int) (mShadowLimit + Math.abs(mDy));

            if (leftShow) {
                leftPading = xPadding;
            } else {
                leftPading = 0;
            }

            if (topShow) {
                topPading = yPadding;
            } else {
                topPading = 0;
            }


            if (rightShow) {
                rightPading = xPadding;
            } else {
                rightPading = 0;
            }

            if (bottomShow) {
                bottomPading = yPadding;
            } else {
                bottomPading = 0;
            }
        } else {
            if (Math.abs(mDy) > mShadowLimit) {
                if (mDy > 0) {
                    mDy = mShadowLimit;
                } else {
                    mDy = 0 - mShadowLimit;
                }
            }


            if (Math.abs(mDx) > mShadowLimit) {
                if (mDx > 0) {
                    mDx = mShadowLimit;
                } else {
                    mDx = 0 - mShadowLimit;
                }
            }

            if (topShow) {
                topPading = (int) (mShadowLimit - mDy);
            } else {
                topPading = 0;
            }

            if (bottomShow) {
                bottomPading = (int) (mShadowLimit + mDy);
            } else {
                bottomPading = 0;
            }


            if (rightShow) {
                rightPading = (int) (mShadowLimit - mDx);
            } else {
                rightPading = 0;
            }


            if (leftShow) {
                leftPading = (int) (mShadowLimit + mDx);
            } else {
                leftPading = 0;
            }
        }


        setPadding(leftPading, topPading, rightPading, bottomPading);
    }


    @SuppressWarnings("deprecation")
    private void setBackgroundCompat(int w, int h) {
        if (isShowShadow) {
            //判断传入的颜色值是否有透明度
            isAddAlpha(mShadowColor);
            Bitmap bitmap = createShadowBitmap(w, h, mCornerRadius, mShadowLimit, mDx, mDy, mShadowColor, Color.TRANSPARENT);
            BitmapDrawable drawable = new BitmapDrawable(bitmap);
            setBackground(drawable);
        } else {
            //解决不执行onDraw方法的bug就是给其设置一个透明色
            this.setBackgroundColor(Color.TRANSPARENT);
        }
    }


    private void initAttributes(AttributeSet attrs) {
        TypedArray attr = getContext().obtainStyledAttributes(attrs, R.styleable.ShadowLayout);
        if (attr == null) {
            return;
        }

        try {
            //默认是显示
            isShowShadow = attr.getBoolean(R.styleable.ShadowLayout_hl_isShowShadow, true);
            leftShow = attr.getBoolean(R.styleable.ShadowLayout_hl_leftShow, true);
            rightShow = attr.getBoolean(R.styleable.ShadowLayout_hl_rightShow, true);
            bottomShow = attr.getBoolean(R.styleable.ShadowLayout_hl_bottomShow, true);
            topShow = attr.getBoolean(R.styleable.ShadowLayout_hl_topShow, true);
            mCornerRadius = attr.getDimension(R.styleable.ShadowLayout_hl_cornerRadius, 0);
            mCornerRadius_leftTop = attr.getDimension(R.styleable.ShadowLayout_hl_cornerRadius_leftTop, -1);
            mCornerRadius_leftBottom = attr.getDimension(R.styleable.ShadowLayout_hl_cornerRadius_leftBottom, -1);
            mCornerRadius_rightTop = attr.getDimension(R.styleable.ShadowLayout_hl_cornerRadius_rigthTop, -1);
            mCornerRadius_rightBottom = attr.getDimension(R.styleable.ShadowLayout_hl_cornerRadius_rightBottom, -1);

            //默认扩散区域宽度
            mShadowLimit = attr.getDimension(R.styleable.ShadowLayout_hl_shadowLimit, ScreenUtils.dp2px(5));

            //x轴偏移量
            mDx = attr.getDimension(R.styleable.ShadowLayout_hl_dx, 0);
            //y轴偏移量
            mDy = attr.getDimension(R.styleable.ShadowLayout_hl_dy, 0);
            mShadowColor = attr.getColor(R.styleable.ShadowLayout_hl_shadowColor, Color.parseColor("#2a000000"));
            mBackGroundColor = attr.getColor(R.styleable.ShadowLayout_hl_shadowBackColor, Color.WHITE);
            mBackGroundColorClicked = attr.getColor(R.styleable.ShadowLayout_hl_shadowBackColorClicked, Color.WHITE);
            if (mBackGroundColorClicked != -1) {
                setClickable(true);
            }
            selectorType = attr.getInt(R.styleable.ShadowLayout_hl_selectorMode, 3);
            isSym = attr.getBoolean(R.styleable.ShadowLayout_hl_isSym, true);
        } finally {
            attr.recycle();
        }
    }


    private Bitmap createShadowBitmap(int shadowWidth, int shadowHeight, float cornerRadius, float shadowRadius,
                                      float dx, float dy, int shadowColor, int fillColor) {
        //优化阴影bitmap大小,将尺寸缩小至原来的1/4。
        dx = dx / 4;
        dy = dy / 4;
        shadowWidth = shadowWidth / 4;
        shadowHeight = shadowHeight / 4;
        cornerRadius = cornerRadius / 4;
        shadowRadius = shadowRadius / 4;

        Bitmap output = Bitmap.createBitmap(shadowWidth, shadowHeight, Bitmap.Config.ARGB_4444);
        Canvas canvas = new Canvas(output);

        //这里缩小limt的是因为，setShadowLayer后会将bitmap扩散到shadowWidth，shadowHeight
        RectF shadowRect = new RectF(
                shadowRadius,
                shadowRadius,
                shadowWidth - shadowRadius,
                shadowHeight - shadowRadius);

        if (isSym) {
            if (dy > 0) {
                shadowRect.top += dy;
                shadowRect.bottom -= dy;
            } else if (dy < 0) {
                shadowRect.top += Math.abs(dy);
                shadowRect.bottom -= Math.abs(dy);
            }

            if (dx > 0) {
                shadowRect.left += dx;
                shadowRect.right -= dx;
            } else if (dx < 0) {

                shadowRect.left += Math.abs(dx);
                shadowRect.right -= Math.abs(dx);
            }
        } else {
            shadowRect.top -= dy;
            shadowRect.bottom -= dy;
            shadowRect.right -= dx;
            shadowRect.left -= dx;
        }


        shadowPaint.setColor(fillColor);
        if (!isInEditMode()) {//dx  dy
            shadowPaint.setShadowLayer(shadowRadius, dx, dy, shadowColor);
        }

        if (mCornerRadius_leftBottom == -1 && mCornerRadius_leftTop == -1 && mCornerRadius_rightTop == -1 && mCornerRadius_rightBottom == -1) {
            //如果没有设置整个属性，那么按原始去画
            canvas.drawRoundRect(shadowRect, cornerRadius, cornerRadius, shadowPaint);
        } else {
            //目前最佳的解决方案
            rectf.left = leftPading;
            rectf.top = topPading;
            rectf.right = getWidth() - rightPading;
            rectf.bottom = getHeight() - bottomPading;
            int trueHeight;
            int heightLength = (getHeight() - bottomPading - topPading);
            int widthLength = getWidth() - rightPading - leftPading;
            if (widthLength > heightLength) {
                trueHeight = heightLength;
            } else {
                trueHeight = widthLength;
            }
            float rate = 0.62f;//0.56
            //只要设置一个后就先按照全部圆角设置
            canvas.drawRoundRect(shadowRect, trueHeight / 2, trueHeight / 2, shadowPaint);
            if (mCornerRadius_leftTop != -1) {
                float rate_left_top = mCornerRadius_leftTop / (trueHeight / 2);
                if (rate_left_top <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.left, shadowRect.top, shadowRect.left + trueHeight / 8, shadowRect.top + trueHeight / 8), mCornerRadius_leftTop / 4, mCornerRadius_leftTop / 4, shadowPaint);
                }
            } else {
                float rate_src = mCornerRadius / (trueHeight / 2);
                if (rate_src <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.left, shadowRect.top, shadowRect.left + trueHeight / 8, shadowRect.top + trueHeight / 8), mCornerRadius / 4, mCornerRadius / 4, shadowPaint);
                }
            }


            if (mCornerRadius_leftBottom != -1) {
                float rate_left_bottom = mCornerRadius_leftBottom / (trueHeight / 2);
                if (rate_left_bottom <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.left, shadowRect.bottom - trueHeight / 8, shadowRect.left + trueHeight / 8, shadowRect.bottom), mCornerRadius_leftBottom / 4, mCornerRadius_leftBottom / 4, shadowPaint);
                }
            } else {
                float rate_src = mCornerRadius / (trueHeight / 2);
                if (rate_src <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.left, shadowRect.bottom - trueHeight / 8, shadowRect.left + trueHeight / 8, shadowRect.bottom), mCornerRadius / 4, mCornerRadius / 4, shadowPaint);
                }
            }


            if (mCornerRadius_rightTop != -1) {
                float rate_right_top = mCornerRadius_rightTop / (trueHeight / 2);
                if (rate_right_top <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.right - trueHeight / 8, shadowRect.top, shadowRect.right, shadowRect.top + trueHeight / 8), mCornerRadius_rightTop / 4, mCornerRadius_rightTop / 4, shadowPaint);
                }
            } else {
                float rate_src = mCornerRadius / (trueHeight / 2);
                if (rate_src <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.right - trueHeight / 8, shadowRect.top, shadowRect.right, shadowRect.top + trueHeight / 8), mCornerRadius / 4, mCornerRadius / 4, shadowPaint);
                }
            }


            if (mCornerRadius_rightBottom != -1) {
                float rate_right_bottom = mCornerRadius_rightBottom / (trueHeight / 2);
                if (rate_right_bottom <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.right - trueHeight / 8, shadowRect.bottom - trueHeight / 8, shadowRect.right, shadowRect.bottom), mCornerRadius_rightBottom / 4, mCornerRadius_rightBottom / 4, shadowPaint);
                }
            } else {
                float rate_src = mCornerRadius / (trueHeight / 2);
                if (rate_src <= rate) {
                    canvas.drawRoundRect(new RectF(shadowRect.right - trueHeight / 8, shadowRect.bottom - trueHeight / 8, shadowRect.right, shadowRect.bottom), mCornerRadius / 4, mCornerRadius / 4, shadowPaint);
                }
            }

        }

        return output;
    }


    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        rectf.left = leftPading;
        rectf.top = topPading;
        rectf.right = getWidth() - rightPading;
        rectf.bottom = getHeight() - bottomPading;
        int trueHeight = (int) (rectf.bottom - rectf.top);
        //如果都为0说明，没有设置特定角，那么按正常绘制
        if (mCornerRadius_leftTop == 0 && mCornerRadius_leftBottom == 0 && mCornerRadius_rightTop == 0 && mCornerRadius_rightBottom == 0) {
            if (mCornerRadius > trueHeight / 2) {
                //画圆角矩形
                canvas.drawRoundRect(rectf, trueHeight / 2, trueHeight / 2, paint);
            } else {
                canvas.drawRoundRect(rectf, mCornerRadius, mCornerRadius, paint);
            }
        } else {
            setSpaceCorner(canvas, trueHeight);
        }

    }


    //这是自定义四个角的方法。
    private void setSpaceCorner(Canvas canvas, int trueHeight) {
        int leftTop;
        int rightTop;
        int rightBottom;
        int leftBottom;
        if (mCornerRadius_leftTop == -1) {
            leftTop = (int) mCornerRadius;
        } else {
            leftTop = (int) mCornerRadius_leftTop;
        }

        if (leftTop > trueHeight / 2) {
            leftTop = trueHeight / 2;
        }

        if (mCornerRadius_rightTop == -1) {
            rightTop = (int) mCornerRadius;
        } else {
            rightTop = (int) mCornerRadius_rightTop;
        }

        if (rightTop > trueHeight / 2) {
            rightTop = trueHeight / 2;
        }

        if (mCornerRadius_rightBottom == -1) {
            rightBottom = (int) mCornerRadius;
        } else {
            rightBottom = (int) mCornerRadius_rightBottom;
        }

        if (rightBottom > trueHeight / 2) {
            rightBottom = trueHeight / 2;
        }


        if (mCornerRadius_leftBottom == -1) {
            leftBottom = (int) mCornerRadius;
        } else {
            leftBottom = (int) mCornerRadius_leftBottom;
        }

        if (leftBottom > trueHeight / 2) {
            leftBottom = trueHeight / 2;
        }

        float[] outerR = new float[]{leftTop, leftTop, rightTop, rightTop, rightBottom, rightBottom, leftBottom, leftBottom};//左上，右上，右下，左下
        ShapeDrawable mDrawables = new ShapeDrawable(new RoundRectShape(outerR, null, null));
        mDrawables.getPaint().setColor(paint.getColor());
        mDrawables.setBounds(leftPading, topPading, getWidth() - rightPading, getHeight() - bottomPading);
        mDrawables.draw(canvas);
    }


    public void isAddAlpha(int color) {
        //获取单签颜色值的透明度，如果没有设置透明度，默认加上#2a
        if (Color.alpha(color) == 255) {
            String red = Integer.toHexString(Color.red(color));
            String green = Integer.toHexString(Color.green(color));
            String blue = Integer.toHexString(Color.blue(color));

            if (red.length() == 1) {
                red = "0" + red;
            }

            if (green.length() == 1) {
                green = "0" + green;
            }

            if (blue.length() == 1) {
                blue = "0" + blue;
            }
            String endColor = "#2a" + red + green + blue;
            mShadowColor = convertToColorInt(endColor);
        }
    }


    public static int convertToColorInt(String argb)
            throws IllegalArgumentException {

        if (!argb.startsWith("#")) {
            argb = "#" + argb;
        }

        return Color.parseColor(argb);
    }


    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mBackGroundColorClicked != -1) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    if (!ShadowLayout.this.isSelected() && selectorType != 2) {
                        paint.setColor(mBackGroundColorClicked);
                        postInvalidate();
                    }
                    break;

                case MotionEvent.ACTION_UP:
                    if (!ShadowLayout.this.isSelected() && selectorType != 2) {
                        paint.setColor(mBackGroundColor);
                        postInvalidate();
                    }
                    break;
            }
        }
        return super.onTouchEvent(event);
    }

}

