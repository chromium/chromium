// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.DualControlLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * Layout that arranges an infobar's views.
 *
 * An InfoBarLayout consists of:
 * - A message describing why the infobar is being displayed.
 * - A close button in the top right corner.
 * - (optional) An icon representing the infobar's purpose in the top left corner.
 * - (optional) Additional {@link InfoBarControlLayouts} for specialized controls (e.g. spinners).
 * - (optional) One or two buttons with text at the bottom, or a button paired with an ImageView.
 *
 * When adding custom views, widths and heights defined in the LayoutParams will be ignored.
 * Setting a minimum width using {@link View#setMininumWidth()} will be obeyed.
 *
 * Logic for what happens when things are clicked should be implemented by the InfoBarView.
 */
public final class InfoBarLayout extends ViewGroup implements View.OnClickListener {

    /**
     * Parameters used for laying out children.
     */
    private static class LayoutParams extends ViewGroup.LayoutParams {
        public int startMargin;
        public int endMargin;
        public int topMargin;
        public int bottomMargin;

        // Where this view will be laid out. Calculated in onMeasure() and used in onLayout().
        public int start;
        public int top;

        LayoutParams(int startMargin, int topMargin, int endMargin, int bottomMargin) {
            super(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            this.startMargin = startMargin;
            this.topMargin = topMargin;
            this.endMargin = endMargin;
            this.bottomMargin = bottomMargin;
        }
    }

    private final int mSmallIconSize;
    private final int mSmallIconMargin;
    private final int mBigIconSize;
    private final int mBigIconMargin;
    private final int mMarginAboveButtonGroup;
    private final int mMarginAboveControlGroups;
    private final int mPadding;
    private final int mMinWidth;

    private final InfoBarView mInfoBarView;
    private final ImageButton mCloseButton;
    private final InfoBarControlLayout mMessageLayout;
    private final List<InfoBarControlLayout> mControlLayouts;

    private TextView mMessageTextView;
    private ImageView mIconView;
    private DualControlLayout mButtonRowLayout;

    private CharSequence mMessageMainText;
    private String mMessageLinkText;
    private int mMessageInlineLinkRangeStart;
    private int mMessageInlineLinkRangeEnd;

    /**
     * Constructs a layout for the specified infobar. After calling this, be sure to set the
     * message, the buttons, and/or the custom content using setMessage(), setButtons(), and
     * setCustomContent().
     *
     * @param context The context used to render.
     * @param infoBarView InfoBarView that listens to events.
     * @param iconResourceId ID of the icon to use for the infobar.
     * @param iconBitmap Bitmap for the icon to use, if the resource ID wasn't passed through.
     * @param message The message to show in the infobar.
     */
    public InfoBarLayout(Context context, InfoBarView infoBarView, int iconResourceId,
            Bitmap iconBitmap, CharSequence message) {
        super(context);
        mControlLayouts = new ArrayList<InfoBarControlLayout>();

        mInfoBarView = infoBarView;

        // Cache resource values.
        Resources res = getResources();
        mSmallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
        mSmallIconMargin = res.getDimensionPixelSize(R.dimen.infobar_small_icon_margin);
        mBigIconSize = res.getDimensionPixelSize(R.dimen.infobar_big_icon_size);
        mBigIconMargin = res.getDimensionPixelSize(R.dimen.infobar_big_icon_margin);
        mMarginAboveButtonGroup =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_button_row);
        mMarginAboveControlGroups =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_control_groups);
        mPadding = res.getDimensionPixelOffset(R.dimen.infobar_padding);
        mMinWidth = res.getDimensionPixelSize(R.dimen.infobar_min_width);

        // Set up the close button. Apply padding so it has a big touch target.
        mCloseButton = createCloseButton(context);
        mCloseButton.setOnClickListener(this);
        mCloseButton.setPadding(mPadding, mPadding, mPadding, mPadding);
        mCloseButton.setLayoutParams(new LayoutParams(0, -mPadding, -mPadding, -mPadding));

        // Set up the icon, if necessary.
        mIconView = createIconView(context, iconResourceId, iconBitmap);
        if (mIconView != null) {
            mIconView.setLayoutParams(new LayoutParams(0, 0, mSmallIconMargin, 0));
            mIconView.getLayoutParams().width = mSmallIconSize;
            mIconView.getLayoutParams().height = mSmallIconSize;
        }

        // Set up the message view.
        mMessageMainText = message;
        mMessageLayout = new InfoBarControlLayout(context);
        mMessageTextView = mMessageLayout.addMainMessage(prepareMainMessageString());
    }

    /**
     * Returns the {@link TextView} corresponding to the main infobar message.
     */
    TextView getMessageTextView() {
        return mMessageTextView;
    }

    /**
     * Returns the {@link InfoBarControlLayout} containing the TextView showing the main infobar
     * message and associated controls, which is sandwiched between its icon and close button.
     */
    InfoBarControlLayout getMessageLayout() {
        return mMessageLayout;
    }

    /**
     * Sets the message to show on the infobar.
     * TODO(dfalcantara): Do some magic here to determine if TextViews need to have line spacing
     *                    manually added.  Android changed when these values were applied between
     *                    KK and L: https://crbug.com/543205
     */
    public void setMessage(CharSequence message) {
        mMessageMainText = message;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Appends a link to the message, if an infobar requires one (e.g. "Learn more").
     */
    public void appendMessageLinkText(String linkText) {
        mMessageLinkText = linkText;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Sets up the message to have an inline link, assuming an inclusive range.
     * @param rangeStart Where the link starts.
     * @param rangeEnd   Where the link ends.
     */
    void setInlineMessageLink(int rangeStart, int rangeEnd) {
        mMessageInlineLinkRangeStart = rangeStart;
        mMessageInlineLinkRangeEnd = rangeEnd;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Adds an {@link InfoBarControlLayout} to house additional infobar controls, like toggles and
     * spinners.
     */
    public InfoBarControlLayout addControlLayout() {
        InfoBarControlLayout controlLayout = new InfoBarControlLayout(getContext());
        mControlLayouts.add(controlLayout);
        return controlLayout;
    }

    /**
     * Adds one or two buttons to the layout.
     *
     * @param primaryText Text for the primary button.  If empty, no buttons are added at all.
     * @param secondaryText Text for the secondary button, or null if there isn't a second button.
     */
    public void setButtons(String primaryText, String secondaryText) {
        if (TextUtils.isEmpty(primaryText)) {
            assert TextUtils.isEmpty(secondaryText);
            return;
        }

        Button secondaryButton = null;
        if (!TextUtils.isEmpty(secondaryText)) {
            secondaryButton = DualControlLayout.createButtonForLayout(
                    getContext(), false, secondaryText, this);
        }

        setBottomViews(
                primaryText, secondaryButton, DualControlLayout.DualControlLayoutAlignment.END);
    }

    /**
     * Sets up the bottom-most part of the infobar with a primary button (e.g. OK) and a secondary
     * View of your choice.  Subclasses should be calling {@link #setButtons(String, String)}
     * instead of this function in nearly all cases (that function calls this one).
     *
     * @param primaryText Text to display on the primary button.  If empty, the bottom layout is not
     *                    created.
     * @param secondaryView View that is aligned with the primary button.  May be null.
     * @param alignment One of ALIGN_START, ALIGN_APART, or ALIGN_END from
     *                  {@link DualControlLayout}.
     */
    public void setBottomViews(String primaryText, View secondaryView, int alignment) {
        assert !TextUtils.isEmpty(primaryText);
        Button primaryButton = DualControlLayout.createButtonForLayout(
                getContext(), true, primaryText, this);

        assert mButtonRowLayout == null;
        mButtonRowLayout = new DualControlLayout(getContext(), null);
        mButtonRowLayout.setAlignment(alignment);
        mButtonRowLayout.setStackedMargin(getResources().getDimensionPixelSize(
                R.dimen.infobar_margin_between_stacked_buttons));

        mButtonRowLayout.addView(primaryButton);
        if (secondaryView != null) mButtonRowLayout.addView(secondaryView);
    }

    /**
     * Adjusts styling to account for the big icon layout.
     */
    public void setIsUsingBigIcon() {
        if (mIconView == null) return;

        LayoutParams lp = (LayoutParams) mIconView.getLayoutParams();
        lp.width = mBigIconSize;
        lp.height = mBigIconSize;
        lp.endMargin = mBigIconMargin;

        Resources res = getContext().getResources();
        float textSize = res.getDimension(R.dimen.infobar_big_icon_message_size);
        mMessageTextView.setTypeface(UiUtils.createRobotoMediumTypeface());
        mMessageTextView.setMaxLines(1);
        mMessageTextView.setEllipsize(TextUtils.TruncateAt.END);
        mMessageTextView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize);
    }

    /**
     * Returns the primary button, or null if it doesn't exist.
     */
    public ButtonCompat getPrimaryButton() {
        return mButtonRowLayout == null ? null
                : (ButtonCompat) mButtonRowLayout.findViewById(R.id.button_primary);
    }

    /**
     * Returns the icon, or null if it doesn't exist.
     */
    public ImageView getIcon() {
        return mIconView;
    }

    /**
     * Must be called after the message, buttons, and custom content have been set, and before the
     * first call to onMeasure().
     */
    void onContentCreated() {
        // Add the child views in the desired focus order.
        if (mIconView != null) addView(mIconView);
        addView(mMessageLayout);
        for (View v : mControlLayouts) addView(v);
        if (mButtonRowLayout != null) addView(mButtonRowLayout);
        addView(mCloseButton);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(0, 0, 0, 0);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Place all the views in the positions already determined during onMeasure().
        int width = right - left;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);

        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int childLeft = lp.start;
            int childRight = lp.start + child.getMeasuredWidth();

            if (isRtl) {
                int tmp = width - childRight;
                childRight = width - childLeft;
                childLeft = tmp;
            }

            child.layout(childLeft, lp.top, childRight, lp.top + child.getMeasuredHeight());
        }
    }

    /**
     * Measures and determines where children should go.
     *
     * For current specs, see https://goto.google.com/infobar-spec
     *
     * All controls are padded from the infobar boundary by the same amount, but different types of
     * control groups are bound by different widths and have different margins:
     * --------------------------------------------------------------------------------
     * |  PADDING                                                                     |
     * |  --------------------------------------------------------------------------  |
     * |  | ICON | MESSAGE LAYOUT                                              | X |  |
     * |  |------+                                                             +---|  |
     * |  |      |                                                             |   |  |
     * |  |      ------------------------------------------------------------------|  |
     * |  |      | CONTROL LAYOUT #1                                               |  |
     * |  |      ------------------------------------------------------------------|  |
     * |  |      | CONTROL LAYOUT #X                                               |  |
     * |  |------------------------------------------------------------------------|  |
     * |  | BOTTOM ROW LAYOUT                                                      |  |
     * |  -------------------------------------------------------------------------|  |
     * |                                                                              |
     * --------------------------------------------------------------------------------
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        assert getLayoutParams().height == LayoutParams.WRAP_CONTENT
                : "InfoBar heights cannot be constrained.";

        // Apply the padding that surrounds all the infobar controls.
        final int layoutWidth = Math.max(MeasureSpec.getSize(widthMeasureSpec), mMinWidth);
        final int paddedStart = mPadding;
        final int paddedEnd = layoutWidth - mPadding;
        int layoutBottom = mPadding;

        // Measure and place the icon in the top-left corner.
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        if (mIconView != null) {
            LayoutParams iconParams = getChildLayoutParams(mIconView);
            measureChild(mIconView, unspecifiedSpec, unspecifiedSpec);
            iconParams.start = paddedStart + iconParams.startMargin;
            iconParams.top = layoutBottom + iconParams.topMargin;
        }
        final int iconWidth = getChildWidthWithMargins(mIconView);

        // Measure and place the close button in the top-right corner of the layout.
        LayoutParams closeParams = getChildLayoutParams(mCloseButton);
        measureChild(mCloseButton, unspecifiedSpec, unspecifiedSpec);
        closeParams.start = paddedEnd - closeParams.endMargin - mCloseButton.getMeasuredWidth();
        closeParams.top = layoutBottom + closeParams.topMargin;

        // Determine how much width is available for all the different control layouts; see the
        // function JavaDoc above for details.
        final int paddedWidth = paddedEnd - paddedStart;
        final int controlLayoutWidth = paddedWidth - iconWidth;
        final int messageWidth = controlLayoutWidth - getChildWidthWithMargins(mCloseButton);

        // The message layout is sandwiched between the icon and the close button.
        LayoutParams messageParams = getChildLayoutParams(mMessageLayout);
        measureChildWithFixedWidth(mMessageLayout, messageWidth);
        messageParams.start = paddedStart + iconWidth;
        messageParams.top = layoutBottom;

        // Control layouts are placed below the message layout and the close button.  The icon is
        // ignored for this particular calculation because the icon enforces a left margin on all of
        // the control layouts and won't be overlapped.
        layoutBottom += Math.max(getChildHeightWithMargins(mMessageLayout),
                getChildHeightWithMargins(mCloseButton));

        // The other control layouts are constrained only by the icon's width.
        final int controlPaddedStart = paddedStart + iconWidth;
        for (int i = 0; i < mControlLayouts.size(); i++) {
            View child = mControlLayouts.get(i);
            measureChildWithFixedWidth(child, controlLayoutWidth);

            layoutBottom += mMarginAboveControlGroups;
            getChildLayoutParams(child).start = controlPaddedStart;
            getChildLayoutParams(child).top = layoutBottom;
            layoutBottom += child.getMeasuredHeight();
        }

        // The button layout takes up the full width of the infobar and sits below everything else,
        // including the icon.
        layoutBottom = Math.max(layoutBottom, getChildHeightWithMargins(mIconView));
        if (mButtonRowLayout != null) {
            measureChildWithFixedWidth(mButtonRowLayout, paddedWidth);

            layoutBottom += mMarginAboveButtonGroup;
            getChildLayoutParams(mButtonRowLayout).start = paddedStart;
            getChildLayoutParams(mButtonRowLayout).top = layoutBottom;
            layoutBottom += mButtonRowLayout.getMeasuredHeight();
        }

        // Apply padding to the bottom of the infobar.
        layoutBottom += mPadding;

        setMeasuredDimension(resolveSize(layoutWidth, widthMeasureSpec),
                resolveSize(layoutBottom, heightMeasureSpec));
    }

    private static int getChildWidthWithMargins(View view) {
        if (view == null) return 0;
        return view.getMeasuredWidth() + getChildLayoutParams(view).startMargin
                + getChildLayoutParams(view).endMargin;
    }

    private static int getChildHeightWithMargins(View view) {
        if (view == null) return 0;
        return view.getMeasuredHeight() + getChildLayoutParams(view).topMargin
                + getChildLayoutParams(view).bottomMargin;
    }

    private static LayoutParams getChildLayoutParams(View view) {
        return (LayoutParams) view.getLayoutParams();
    }

    /**
     * Measures a child for the given space, taking into account its margins.
     */
    private void measureChildWithFixedWidth(View child, int width) {
        LayoutParams lp = getChildLayoutParams(child);
        int availableWidth = width - lp.startMargin - lp.endMargin;
        int widthSpec = MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        child.measure(widthSpec, heightSpec);
    }

    /**
     * Listens for View clicks.
     * Classes that override this function MUST call this one.
     * @param view View that was clicked on.
     */
    @Override
    public void onClick(View view) {
        mInfoBarView.setControlsEnabled(false);

        if (view.getId() == R.id.infobar_close_button) {
            mInfoBarView.onCloseButtonClicked();
        } else if (view.getId() == R.id.button_primary) {
            mInfoBarView.onButtonClicked(true);
        } else if (view.getId() == R.id.button_secondary) {
            mInfoBarView.onButtonClicked(false);
        }
    }

    /**
     * Prepares text to be displayed as the infobar's main message, including setting up a
     * clickable link if the infobar requires it.
     */
    private CharSequence prepareMainMessageString() {
        SpannableStringBuilder fullString = new SpannableStringBuilder();

        if (!TextUtils.isEmpty(mMessageMainText)) {
            SpannableString spannedMessage = new SpannableString(mMessageMainText);

            // If there's an inline link, apply the necessary span for it.
            if (mMessageInlineLinkRangeEnd != 0) {
                assert mMessageInlineLinkRangeStart < mMessageInlineLinkRangeEnd;
                assert mMessageInlineLinkRangeEnd < mMessageMainText.length();

                spannedMessage.setSpan(createClickableSpan(), mMessageInlineLinkRangeStart,
                        mMessageInlineLinkRangeEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            }

            fullString.append(spannedMessage);
        }

        // Concatenate the text to display for the link and make it clickable.
        if (!TextUtils.isEmpty(mMessageLinkText)) {
            if (fullString.length() > 0) fullString.append(" ");
            int spanStart = fullString.length();

            fullString.append(mMessageLinkText);
            fullString.setSpan(createClickableSpan(), spanStart, fullString.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        return fullString;
    }

    private NoUnderlineClickableSpan createClickableSpan() {
        return new NoUnderlineClickableSpan((view) -> mInfoBarView.onLinkClicked());
    }

    /**
     * Creates a View that holds an icon representing an infobar.
     * @param context Context to grab resources from.
     * @param iconResourceId ID of the icon to use for the infobar.
     * @param iconBitmap Bitmap for the icon to use, if the resource ID wasn't passed through.
     * @return {@link ImageButton} that represents the icon.
     */
    @Nullable
    static ImageView createIconView(Context context, int iconResourceId, Bitmap iconBitmap) {
        ImageView iconView = null;
        if (iconResourceId != 0 || iconBitmap != null) {
            iconView = new ImageView(context);
            if (iconResourceId != 0) {
                iconView.setImageDrawable(AppCompatResources.getDrawable(context, iconResourceId));
            } else if (iconBitmap != null) {
                iconView.setImageBitmap(iconBitmap);
            }
            iconView.setFocusable(false);
            iconView.setId(R.id.infobar_icon);
            iconView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        }
        return iconView;
    }

    /**
     * Creates a close button that can be inserted into an infobar.
     * @param context Context to grab resources from.
     * @return {@link ImageButton} that represents a close button.
     */
    static ImageButton createCloseButton(Context context) {
        TypedArray a = context.obtainStyledAttributes(new int[] {R.attr.selectableItemBackground});
        Drawable closeButtonBackground = a.getDrawable(0);
        a.recycle();

        ImageButton closeButton = new ImageButton(context);
        closeButton.setId(R.id.infobar_close_button);
        closeButton.setImageResource(R.drawable.btn_close);
        closeButton.setBackground(closeButtonBackground);
        closeButton.setContentDescription(context.getString(R.string.infobar_close));
        closeButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

        return closeButton;
    }
}
