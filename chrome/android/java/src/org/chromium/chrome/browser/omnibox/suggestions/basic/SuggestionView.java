// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Container view for omnibox suggestions made very specific for omnibox suggestions to minimize
 * any unnecessary measures and layouts.
 */
@VisibleForTesting
public class SuggestionView extends ViewGroup implements OnClickListener {
    private static final float ANSWER_IMAGE_SCALING_FACTOR = 1.15f;
    @IntDef({SuggestionIconType.FALLBACK, SuggestionIconType.FAVICON})
    @Retention(RetentionPolicy.SOURCE)
    @interface SuggestionIconType {
        int FALLBACK = 0;
        int FAVICON = 1;
    }

    private final int mSuggestionHeight;
    private final int mSuggestionAnswerHeight;
    private final int mSuggestionMaxHeight;
    private SuggestionViewDelegate mSuggestionDelegate;

    private int mRefineViewOffsetPx;

    private final SuggestionContentsContainer mContentsView;

    private final int mRefineWidth;
    private final View mRefineView;
    private TintedDrawable mRefineIcon;

    private boolean mUseDarkIconTint;

    // Pre-computed offsets in px.
    private int mSuggestionIconAreaSizePx;
    private int mSuggestionFaviconSizePx;
    // Current suggestion icon dimensions.
    private int mSuggestionIconWidthPx;
    private int mSuggestionIconHeightPx;

    /**
     * Constructs a new omnibox suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     */
    public SuggestionView(Context context) {
        super(context);

        setOnClickListener(this);

        mSuggestionHeight =
                context.getResources().getDimensionPixelOffset(R.dimen.omnibox_suggestion_height);
        mSuggestionAnswerHeight = context.getResources().getDimensionPixelOffset(
                R.dimen.omnibox_suggestion_answer_height);
        mSuggestionMaxHeight = context.getResources().getDimensionPixelOffset(
                R.dimen.omnibox_suggestion_max_height);

        TypedArray a =
                getContext().obtainStyledAttributes(new int[] {R.attr.selectableItemBackground});
        Drawable itemBackground = a.getDrawable(0);
        a.recycle();

        mContentsView = new SuggestionContentsContainer(context, itemBackground);
        addView(mContentsView);

        mRefineView = new View(context) {
            @Override
            protected void onDraw(Canvas canvas) {
                super.onDraw(canvas);

                if (mRefineIcon == null) return;
                canvas.save();
                canvas.translate((getMeasuredWidth() - mRefineIcon.getIntrinsicWidth()) / 2f,
                        (getMeasuredHeight() - mRefineIcon.getIntrinsicHeight()) / 2f);
                mRefineIcon.draw(canvas);
                canvas.restore();
            }

            @Override
            public void setVisibility(int visibility) {
                super.setVisibility(visibility);

                if (visibility == VISIBLE) {
                    setClickable(true);
                    setFocusable(true);
                } else {
                    setClickable(false);
                    setFocusable(false);
                }
            }

            @Override
            protected void drawableStateChanged() {
                super.drawableStateChanged();

                if (mRefineIcon != null && mRefineIcon.isStateful()) {
                    mRefineIcon.setState(getDrawableState());
                }
            }
        };
        mRefineView.setContentDescription(
                getContext().getString(R.string.accessibility_omnibox_btn_refine));

        // Although this has the same background as the suggestion view, it can not be shared as
        // it will result in the state of the drawable being shared and always showing up in the
        // refine view.
        mRefineView.setBackground(itemBackground.getConstantState().newDrawable());
        mRefineView.setId(R.id.refine_view_id);
        mRefineView.setClickable(true);
        mRefineView.setFocusable(true);
        mRefineView.setLayoutParams(new LayoutParams(0, 0));
        addView(mRefineView);

        mRefineWidth =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_refine_width);
        mRefineViewOffsetPx = getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);
        mSuggestionFaviconSizePx =
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);
        setSuggestionIconAreaWidthRes(R.dimen.omnibox_suggestion_start_offset_without_icon);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        if (getMeasuredWidth() == 0) return;

        boolean refineVisible = mRefineView.getVisibility() == VISIBLE;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        int contentsViewOffsetX = isRtl && refineVisible ? mRefineWidth : 0;
        mContentsView.layout(contentsViewOffsetX, 0,
                contentsViewOffsetX + mContentsView.getMeasuredWidth(),
                mContentsView.getMeasuredHeight());

        int refineViewOffsetX = isRtl ? mRefineViewOffsetPx
                                      : (getMeasuredWidth() - mRefineWidth) - mRefineViewOffsetPx;
        mRefineView.layout(refineViewOffsetX, 0, refineViewOffsetX + mRefineWidth,
                mContentsView.getMeasuredHeight());
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = mSuggestionHeight;
        boolean refineVisible = mRefineView.getVisibility() == VISIBLE;
        int refineWidth = refineVisible ? mRefineWidth : 0;
        setMeasuredDimension(width, height);

        // The width will be specified as 0 when determining the height of the popup, so exit early
        // after setting the height.
        if (width == 0) return;

        mContentsView.measure(MeasureSpec.makeMeasureSpec(width - refineWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));

        mContentsView.getLayoutParams().width = mContentsView.getMeasuredWidth();
        mContentsView.getLayoutParams().height = mContentsView.getMeasuredHeight();

        mRefineView.measure(MeasureSpec.makeMeasureSpec(mRefineWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        mRefineView.getLayoutParams().width = mRefineView.getMeasuredWidth();
        mRefineView.getLayoutParams().height = mRefineView.getMeasuredHeight();
    }

    @Override
    public void invalidate() {
        super.invalidate();
        mContentsView.invalidate();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // Whenever the suggestion dropdown is touched, we dispatch onGestureDown which is
        // used to let autocomplete controller know that it should stop updating suggestions.
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) mSuggestionDelegate.onGestureDown();
        if (ev.getActionMasked() == MotionEvent.ACTION_UP) {
            mSuggestionDelegate.onGestureUp(ev.getEventTime());
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public void onClick(View v) {
        mContentsView.callOnClick();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (KeyNavigationUtil.isGoRight(event)) {
            mSuggestionDelegate.onRefineSuggestion();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    /** Sets the delegate for the actions on the suggestion view. */
    void setDelegate(SuggestionViewDelegate delegate) {
        mSuggestionDelegate = delegate;
    }

    /** Get the View containing the first line of text. */
    TextView getTextLine1() {
        return mContentsView.mTextLine1;
    }

    /** Get the View containing the second line of text. */
    TextView getTextLine2() {
        return mContentsView.mTextLine2;
    }

    /**
     * Specify area width for the suggestion icon.
     * This call generally controls suggestion icon visibility.
     * @param areaWidthRes Dimen resource specifying area width.
     */
    void setSuggestionIconAreaWidthRes(@DimenRes int areaWidthRes) {
        mSuggestionIconAreaSizePx = getResources().getDimensionPixelOffset(areaWidthRes);
    }

    void setRefinable(boolean refinable) {
        if (refinable) {
            mRefineView.setVisibility(VISIBLE);
            mRefineView.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Post the refine action to the end of the UI thread to allow the refine view
                    // a chance to update its background selection state.
                    PerformRefineSuggestion performRefine = new PerformRefineSuggestion();
                    if (!post(performRefine)) performRefine.run();
                }
            });
        } else {
            mRefineView.setOnClickListener(null);
            mRefineView.setVisibility(GONE);
        }
    }

    /**
     * Initializes the refine icon and sets it to the specified tint.
     */
    void initRefineIcon(boolean useDarkColors) {
        if (mRefineIcon != null) return;
        @ColorRes
        int tintId = ChromeColors.getIconTintRes(!useDarkColors);
        mRefineIcon = TintedDrawable.constructTintedDrawable(
                getContext(), R.drawable.btn_suggestion_refine, tintId);
        mRefineIcon.setBounds(
                0, 0, mRefineIcon.getIntrinsicWidth(), mRefineIcon.getIntrinsicHeight());
        mRefineIcon.setState(mRefineView.getDrawableState());
        mRefineView.postInvalidateOnAnimation();
    }

    /**
     * Updates the refine icon (if present) to use the specified tint.
     */
    void updateRefineIconTint(boolean useDarkColors) {
        if (mRefineIcon == null) return;
        @ColorRes
        int tintId = ChromeColors.getIconTintRes(!useDarkColors);
        mRefineIcon.setTint(AppCompatResources.getColorStateList(getContext(), tintId));
        mRefineView.postInvalidateOnAnimation();
    }

    /**
     * Updates the suggestion icon to the specified drawable with the specified tint.
     * @param icon Drawable instance.
     * @param type Type / function of the supplied icon.
     * @param allowTint specifies whether icon should receive a tint or displayed as is.
     * @param useDarkTint specifies whether to apply dark or light tint to an icon
     */
    void setSuggestionIconDrawable(
            Drawable icon, @SuggestionIconType int type, boolean allowTint, boolean useDarkTint) {
        if (type == SuggestionIconType.FALLBACK) {
            mSuggestionIconWidthPx = icon.getIntrinsicWidth();
            mSuggestionIconHeightPx = icon.getIntrinsicHeight();
        } else {
            mSuggestionIconWidthPx = mSuggestionFaviconSizePx;
            mSuggestionIconHeightPx = mSuggestionFaviconSizePx;
        }

        mContentsView.mSuggestionIcon = icon;
        mContentsView.mAllowTint = allowTint;
        mUseDarkIconTint = useDarkTint;
        icon.setBounds(0, 0, mSuggestionIconWidthPx, mSuggestionIconHeightPx);
        updateSuggestionIconTint(useDarkTint);
        mContentsView.invalidate();
    }

    /**
     * Updates the suggestion icon (if present) to use the specified tint.
     */
    void updateSuggestionIconTint(boolean useDarkTint) {
        if (!mContentsView.mAllowTint || mContentsView.mSuggestionIcon == null) return;
        DrawableCompat.setTint(mContentsView.mSuggestionIcon,
                ApiCompatibilityUtils.getColor(getContext().getResources(),
                        useDarkTint ? R.color.default_icon_color_secondary_list
                                    : R.color.white_mode_tint));
        mContentsView.invalidate();
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        if (selected && !isInTouchMode()) {
            mSuggestionDelegate.onSetUrlToSuggestion();
        }
    }

    /**
     * Handles triggering a selection request for the suggestion rendered by this view.
     */
    private class PerformSelectSuggestion implements Runnable {
        @Override
        public void run() {
            mSuggestionDelegate.onSelection();
        }
    }

    /**
     * Handles triggering a refine request for the suggestion rendered by this view.
     */
    private class PerformRefineSuggestion implements Runnable {
        @Override
        public void run() {
            mSuggestionDelegate.onRefineSuggestion();
        }
    }

    /**
     * Container view for the contents of the suggestion (the search query, URL, and suggestion type
     * icon).
     */
    private class SuggestionContentsContainer extends ViewGroup {
        private Drawable mSuggestionIcon;
        private boolean mAllowTint;

        private final TextView mTextLine1;
        private final TextView mTextLine2;

        private boolean mForceIsFocused;

        // TODO(crbug.com/635567): Fix this properly.
        @SuppressLint("InlinedApi")
        SuggestionContentsContainer(Context context, Drawable backgroundDrawable) {
            super(context);

            setLayoutDirection(View.LAYOUT_DIRECTION_INHERIT);

            setBackground(backgroundDrawable);
            setClickable(true);
            setFocusable(true);
            setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, mSuggestionHeight));
            setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Post the selection action to the end of the UI thread to allow the suggestion
                    // view a chance to update their background selection state.
                    PerformSelectSuggestion performSelection = new PerformSelectSuggestion();
                    if (!post(performSelection)) performSelection.run();
                }
            });
            setOnLongClickListener(new OnLongClickListener() {
                @Override
                public boolean onLongClick(View v) {
                    mSuggestionDelegate.onLongPress();
                    return true;
                }
            });

            mTextLine1 = new TextView(context);
            mTextLine1.setLayoutParams(
                    new LayoutParams(LayoutParams.WRAP_CONTENT, mSuggestionHeight));
            mTextLine1.setSingleLine();
            mTextLine1.setTextAlignment(TEXT_ALIGNMENT_VIEW_START);
            addView(mTextLine1);

            mTextLine2 = new TextView(context);
            mTextLine2.setLayoutParams(
                    new LayoutParams(LayoutParams.WRAP_CONTENT, mSuggestionHeight));
            mTextLine2.setSingleLine();
            mTextLine2.setVisibility(INVISIBLE);
            mTextLine2.setTextAlignment(TEXT_ALIGNMENT_VIEW_START);
            addView(mTextLine2);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);

            if (mSuggestionIcon != null) {
                canvas.save();
                float suggestionIconLeft =
                        (mSuggestionIconAreaSizePx - mSuggestionIconWidthPx) / 2f;
                if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
                    suggestionIconLeft += getMeasuredWidth() - mSuggestionIconAreaSizePx;
                }
                float suggestionIconTop = (getMeasuredHeight() - mSuggestionIconHeightPx) / 2f;
                canvas.translate(suggestionIconLeft, suggestionIconTop);
                mSuggestionIcon.draw(canvas);
                canvas.restore();
            }
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            final int height = getMeasuredHeight();
            final int line1Height = mTextLine1.getMeasuredHeight();
            final int line2Height =
                    mTextLine2.getVisibility() == VISIBLE ? mTextLine2.getMeasuredHeight() : 0;

            // Center the text lines vertically.
            int line1VerticalOffset = Math.max(0, (height - line1Height - line2Height) / 2);
            // When one line is larger than the other, it contains extra vertical padding. This
            // produces more apparent whitespace above or below the text lines.  Add a small
            // offset to compensate.
            if (line1Height != line2Height) {
                line1VerticalOffset += (line2Height - line1Height) / 10;
            }

            int line2VerticalOffset = line1VerticalOffset + line1Height;
            int answerVerticalOffset = line2VerticalOffset;

            // The text lines total height is larger than this view, snap them to the top and
            // bottom of the view.
            if (line2VerticalOffset + line2Height > height) {
                // The text lines total height is larger than this view, snap them to the top and
                // bottom of the view.
                line1VerticalOffset = 0;
                line2VerticalOffset = height - line2Height;
                answerVerticalOffset = line2VerticalOffset;
            }

            final int line1Top = t + line1VerticalOffset;
            final int line1Bottom = line1Top + line1Height;
            final int line2Top = t + line2VerticalOffset;
            final int line2Bottom = line2Top + line2Height;
            final int line1AdditionalStartPadding =
                    mSuggestionDelegate.getAdditionalTextLine1StartPadding(
                            mTextLine1, r - l - mSuggestionIconAreaSizePx);

            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
                int rightStartPos = r - l - mSuggestionIconAreaSizePx;
                mTextLine1.layout(
                        0, line1Top, rightStartPos - line1AdditionalStartPadding, line1Bottom);
                mTextLine2.layout(0, line2Top, rightStartPos, line2Bottom);
            } else {
                mTextLine1.layout(mSuggestionIconAreaSizePx + line1AdditionalStartPadding, line1Top,
                        r - l, line1Bottom);
                mTextLine2.layout(mSuggestionIconAreaSizePx, line2Top, r - l, line2Bottom);
            }
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            // TODO(tedchoc): Instead of comparing width/height, compare the last text (including
            //                style spans) measured and if that remains the same along with the
            //                height/width of this view, then we should be able to skip measure
            //                properly.
            int maxWidth = MeasureSpec.getSize(widthMeasureSpec) - mSuggestionIconAreaSizePx;
            mTextLine1.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(mSuggestionHeight, MeasureSpec.AT_MOST));
            mTextLine2.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(mSuggestionHeight, MeasureSpec.AT_MOST));

            if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.AT_MOST) {
                int desiredHeight = mTextLine1.getMeasuredHeight() + mTextLine2.getMeasuredHeight();
                int additionalPadding = (int) getResources().getDimension(
                        R.dimen.omnibox_suggestion_text_vertical_padding);
                desiredHeight += additionalPadding;
                desiredHeight = Math.min(MeasureSpec.getSize(heightMeasureSpec), desiredHeight);
                super.onMeasure(widthMeasureSpec,
                        MeasureSpec.makeMeasureSpec(desiredHeight, MeasureSpec.EXACTLY));
            } else {
                assert MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY;
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }

        @Override
        public boolean isFocused() {
            return mForceIsFocused || super.isFocused();
        }

        @Override
        protected int[] onCreateDrawableState(int extraSpace) {
            // When creating the drawable states, treat selected as focused to get the proper
            // highlight when in non-touch mode (i.e. physical keyboard).  This is because only
            // a single view in a window can have focus, and these will only appear if
            // the omnibox has focus, so we trick the drawable state into believing it has it.
            mForceIsFocused = isSelected() && !isInTouchMode();
            int[] drawableState = super.onCreateDrawableState(extraSpace);
            mForceIsFocused = false;
            return drawableState;
        }
    }
}
