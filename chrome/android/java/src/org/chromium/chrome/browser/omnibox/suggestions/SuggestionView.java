// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxResultsAdapter.OmniboxResultItem;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxResultsAdapter.OmniboxSuggestionDelegate;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Container view for omnibox suggestions made very specific for omnibox suggestions to minimize
 * any unnecessary measures and layouts.
 */
@VisibleForTesting
public class SuggestionView extends ViewGroup {
    private static final float ANSWER_IMAGE_SCALING_FACTOR = 1.15f;

    @IntDef({SuggestionLayoutType.TEXT_SUGGESTION, SuggestionLayoutType.ANSWER,
            SuggestionLayoutType.MULTI_LINE_ANSWER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionLayoutType {
        int TEXT_SUGGESTION = 0;
        int ANSWER = 1;
        int MULTI_LINE_ANSWER = 2;
    }

    private final int mSuggestionHeight;
    private final int mSuggestionAnswerHeight;
    @SuggestionLayoutType
    private int mSuggestionLayoutType;

    private OmniboxSuggestion mSuggestion;
    private OmniboxSuggestionDelegate mSuggestionDelegate;
    private int mPosition;
    private int mRefineViewOffsetPx;

    private final SuggestionContentsContainer mContentsView;

    private final int mRefineWidth;
    private final View mRefineView;
    private TintedDrawable mRefineIcon;

    // Pre-computed offsets in px.
    private final int mSuggestionStartOffsetPx;
    private final int mSuggestionIconWidthPx;

    /**
     * Constructs a new omnibox suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     */
    public SuggestionView(Context context) {
        super(context);

        mSuggestionHeight =
                context.getResources().getDimensionPixelOffset(R.dimen.omnibox_suggestion_height);
        mSuggestionAnswerHeight = context.getResources().getDimensionPixelOffset(
                R.dimen.omnibox_suggestion_answer_height);

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

        mSuggestionStartOffsetPx =
                getResources().getDimensionPixelOffset(R.dimen.omnibox_suggestion_start_offset);
        mSuggestionIconWidthPx =
                getResources().getDimensionPixelSize(R.dimen.location_bar_icon_width);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        if (getMeasuredWidth() == 0) return;

        boolean refineVisible = mRefineView.getVisibility() == VISIBLE;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);
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
        if (mSuggestionLayoutType == SuggestionLayoutType.MULTI_LINE_ANSWER) {
            mContentsView.measure(
                    MeasureSpec.makeMeasureSpec(width - refineWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(mSuggestionAnswerHeight * 2, MeasureSpec.AT_MOST));
            height = mContentsView.getMeasuredHeight();
        } else if (mSuggestionLayoutType == SuggestionLayoutType.ANSWER) {
            height = mSuggestionAnswerHeight;
        }
        setMeasuredDimension(width, height);

        // The width will be specified as 0 when determining the height of the popup, so exit early
        // after setting the height.
        if (width == 0) return;

        if (mSuggestionLayoutType != SuggestionLayoutType.MULTI_LINE_ANSWER) {
            mContentsView.measure(
                    MeasureSpec.makeMeasureSpec(width - refineWidth, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        }
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

    /**
     * Sets the contents and state of the view for the given suggestion.
     *
     * @param suggestionItem The omnibox suggestion item this view represents.
     * @param suggestionDelegate The suggestion delegate.
     * @param position Position of the suggestion in the dropdown list.
     */
    // TODO(tedchoc): Remove this.
    public void init(OmniboxResultItem suggestionItem, OmniboxSuggestionDelegate suggestionDelegate,
            int position) {
        // Update the position unconditionally.
        mPosition = position;
        jumpDrawablesToCurrentState();

        mSuggestion = suggestionItem.getSuggestion();
        mSuggestionDelegate = suggestionDelegate;
    }

    /** Set the type of layout this view is rendering. */
    void setSuggestionLayoutType(@SuggestionLayoutType int type) {
        mSuggestionLayoutType = type;
    }

    /** Get the View containing the first line of text. */
    TextView getTextLine1() {
        return mContentsView.mTextLine1;
    }

    /** Get the View containing the second line of text. */
    TextView getTextLine2() {
        return mContentsView.mTextLine2;
    }

    /** Get the View containing the answer image to be shown. */
    ImageView getAnswerImageView() {
        return mContentsView.mAnswerImage;
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
        int tintId = useDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint;
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
        int tintId = useDarkColors ? R.color.dark_mode_tint : R.color.light_mode_tint;
        mRefineIcon.setTint(AppCompatResources.getColorStateList(getContext(), tintId));
        mRefineView.postInvalidateOnAnimation();
    }

    /**
     * Updates the suggestion icon to the specified drawable with the specified tint.
     */
    void setSuggestionIconDrawable(@DrawableRes int resId, boolean useDarkTint) {
        mContentsView.mSuggestionIcon = TintedDrawable.constructTintedDrawable(getContext(), resId,
                useDarkTint ? R.color.dark_mode_tint : R.color.white_mode_tint);
        mContentsView.mSuggestionIcon.setBounds(0, 0,
                mContentsView.mSuggestionIcon.getIntrinsicWidth(),
                mContentsView.mSuggestionIcon.getIntrinsicHeight());
        mContentsView.invalidate();
    }

    /**
     * Updates the suggestion icon (if present) to use the specified tint.
     */
    void updateSuggestionIconTint(boolean useDarkTint) {
        if (mContentsView.mSuggestionIcon == null) return;
        mContentsView.mSuggestionIcon.setTint(AppCompatResources.getColorStateList(
                getContext(), useDarkTint ? R.color.dark_mode_tint : R.color.white_mode_tint));
        mContentsView.invalidate();
    }

    /**
     * Updates the text alignment constraints to be applied when positioning the text.
     */
    void updateTextAlignmentConstraintWidths(float requiredWidth, float matchContentWidth) {
        mContentsView.mRequiredWidth = requiredWidth;
        mContentsView.mMatchContentsWidth = matchContentWidth;
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        if (selected && !isInTouchMode()) {
            mSuggestionDelegate.onSetUrlToSuggestion(mSuggestion);
        }
    }

    /**
     * Handles triggering a selection request for the suggestion rendered by this view.
     */
    private class PerformSelectSuggestion implements Runnable {
        @Override
        public void run() {
            mSuggestionDelegate.onSelection(mSuggestion, mPosition);
        }
    }

    /**
     * Handles triggering a refine request for the suggestion rendered by this view.
     */
    private class PerformRefineSuggestion implements Runnable {
        @Override
        public void run() {
            mSuggestionDelegate.onRefineSuggestion(mSuggestion);
        }
    }

    /**
     * Container view for the contents of the suggestion (the search query, URL, and suggestion type
     * icon).
     */
    private class SuggestionContentsContainer extends ViewGroup {
        private int mTextStart = Integer.MIN_VALUE;
        private TintedDrawable mSuggestionIcon;

        private final TextView mTextLine1;
        private final TextView mTextLine2;
        private final ImageView mAnswerImage;

        private float mRequiredWidth;
        private float mMatchContentsWidth;
        private boolean mForceIsFocused;

        // TODO(crbug.com/635567): Fix this properly.
        @SuppressLint("InlinedApi")
        SuggestionContentsContainer(Context context, Drawable backgroundDrawable) {
            super(context);

            ApiCompatibilityUtils.setLayoutDirection(this, View.LAYOUT_DIRECTION_INHERIT);

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
                    mSuggestionDelegate.onLongPress(mSuggestion, mPosition);
                    return true;
                }
            });

            mTextLine1 = new TextView(context);
            mTextLine1.setLayoutParams(
                    new LayoutParams(LayoutParams.WRAP_CONTENT, mSuggestionHeight));
            mTextLine1.setSingleLine();
            ApiCompatibilityUtils.setTextAlignment(mTextLine1, TEXT_ALIGNMENT_VIEW_START);
            addView(mTextLine1);

            mTextLine2 = new TextView(context);
            mTextLine2.setLayoutParams(
                    new LayoutParams(LayoutParams.WRAP_CONTENT, mSuggestionHeight));
            mTextLine2.setSingleLine();
            mTextLine2.setVisibility(INVISIBLE);
            ApiCompatibilityUtils.setTextAlignment(mTextLine2, TEXT_ALIGNMENT_VIEW_START);
            addView(mTextLine2);

            mAnswerImage = new ImageView(context);
            mAnswerImage.setVisibility(GONE);
            mAnswerImage.setScaleType(ImageView.ScaleType.FIT_CENTER);
            addView(mAnswerImage);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);

            if (mSuggestionIcon != null) {
                canvas.save();
                float suggestionIconLeft =
                        (mSuggestionIconWidthPx - mSuggestionIcon.getIntrinsicWidth()) / 2f;
                if (ApiCompatibilityUtils.isLayoutRtl(this)) {
                    suggestionIconLeft += getMeasuredWidth() - mSuggestionIconWidthPx;
                }
                float suggestionIconTop =
                        (getMeasuredHeight() - mSuggestionIcon.getIntrinsicHeight()) / 2f;
                canvas.translate(suggestionIconLeft, suggestionIconTop);
                mSuggestionIcon.draw(canvas);
                canvas.restore();
            }
        }

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {
            // Align the text to be pixel perfectly aligned with the text in the url bar.
            boolean isRTL = ApiCompatibilityUtils.isLayoutRtl(this);
            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
                int textWidth = isRTL ? mTextStart : (r - l - mTextStart);
                final float maxRequiredWidth = mSuggestionDelegate.getMaxRequiredWidth();
                final float maxMatchContentsWidth = mSuggestionDelegate.getMaxMatchContentsWidth();
                float paddingStart = (textWidth > maxRequiredWidth)
                        ? (mRequiredWidth - mMatchContentsWidth)
                        : Math.max(textWidth - maxMatchContentsWidth, 0);
                // TODO(skanuj) : Change to ViewCompat.getPaddingEnd(...).
                ViewCompat.setPaddingRelative(mTextLine1, (int) paddingStart,
                        mTextLine1.getPaddingTop(), 0, mTextLine1.getPaddingBottom());
            }

            int imageWidth = 0;
            int imageSpacing = 0;
            if (mAnswerImage.getVisibility() == VISIBLE) {
                imageWidth = mAnswerImage.getMeasuredWidth();
                if (imageWidth > 0) {
                    imageSpacing = getResources().getDimensionPixelOffset(
                            R.dimen.omnibox_suggestion_answer_image_horizontal_spacing);
                }
            }

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
            if (mAnswerImage.getVisibility() == VISIBLE) {
                // The image is positioned vertically aligned with the second text line but
                // requires a small additional offset to align with the ascent of the text
                // instead of the top of the text which includes some whitespace.
                answerVerticalOffset += getResources().getDimensionPixelOffset(
                        R.dimen.omnibox_suggestion_answer_image_vertical_spacing);

                line2VerticalOffset += getResources().getDimensionPixelOffset(
                        R.dimen.omnibox_suggestion_answer_line2_vertical_spacing);
            }

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
            final int answerImageTop = t + answerVerticalOffset;
            final int answerImageBottom = answerImageTop + mAnswerImage.getMeasuredHeight();

            if (isRTL) {
                int rightStartPos = r - l - mTextStart;
                mTextLine1.layout(0, line1Top, rightStartPos, line1Bottom);
                mAnswerImage.layout(rightStartPos - imageWidth, answerImageTop, rightStartPos,
                        answerImageBottom);
                mTextLine2.layout(
                        0, line2Top, rightStartPos - (imageWidth + imageSpacing), line2Bottom);
            } else {
                mTextLine1.layout(mTextStart, line1Top, r - l, line1Bottom);
                mAnswerImage.layout(
                        mTextStart, answerImageTop, mTextStart + imageWidth, answerImageBottom);
                mTextLine2.layout(
                        mTextStart + imageWidth + imageSpacing, line2Top, r - l, line2Bottom);
            }
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            mTextStart = mSuggestionStartOffsetPx;

            // TODO(tedchoc): Instead of comparing width/height, compare the last text (including
            //                style spans) measured and if that remains the same along with the
            //                height/width of this view, then we should be able to skip measure
            //                properly.
            int maxWidth = MeasureSpec.getSize(widthMeasureSpec) - mTextStart;
            mTextLine1.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(mSuggestionHeight, MeasureSpec.AT_MOST));
            mTextLine2.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(mSuggestionHeight, MeasureSpec.AT_MOST));

            if (mAnswerImage.getVisibility() == VISIBLE) {
                float textSize = mContentsView.mTextLine2.getTextSize();
                int imageSize = (int) (textSize * ANSWER_IMAGE_SCALING_FACTOR);
                mAnswerImage.measure(MeasureSpec.makeMeasureSpec(imageSize, MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(imageSize, MeasureSpec.EXACTLY));
            }

            if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.AT_MOST) {
                int desiredHeight = mTextLine1.getMeasuredHeight() + mTextLine2.getMeasuredHeight();
                int additionalPadding = (int) getResources().getDimension(
                        R.dimen.omnibox_suggestion_text_vertical_padding);
                if (mSuggestionLayoutType != SuggestionLayoutType.TEXT_SUGGESTION) {
                    additionalPadding += (int) getResources().getDimension(
                            R.dimen.omnibox_suggestion_multiline_text_vertical_padding);
                }
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
