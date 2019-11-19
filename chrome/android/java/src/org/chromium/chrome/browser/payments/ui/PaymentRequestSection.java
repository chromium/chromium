// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v7.widget.GridLayout;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.DualControlLayout;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.ui.HorizontalListDividerDrawable;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents a single section in the {@link PaymentRequestUI} that flips between multiple states.
 *
 * The row is broken up into three major, vertically-centered sections:
 * .............................................................................................
 * . TITLE                                                          |                | CHEVRON .
 * .................................................................|                |    or   .
 * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT |           LOGO |   ADD   .
 * .................................................................|                |    or   .
 * . MAIN SECTION CONTENT                                           |                |  CHOOSE .
 * .............................................................................................
 *
 * 1) MAIN CONTENT
 *    The main content is on the left side of the UI.  This includes the title of the section and
 *    two bits of optional summary text.  Subclasses may extend this class to append more controls
 *    via the {@link #createMainSectionContent} function.
 *
 * 2) LOGO
 *    Displays an optional logo (e.g. a credit card image) that floats to the right of the main
 *    content.
 *
 * 3) CHEVRON or ADD or CHOOSE
 *    Drawn to indicate that the current section may be expanded.  Displayed only when the view is
 *    in the {@link #DISPLAY_MODE_EXPANDABLE} state and only if an ADD or CHOOSE button isn't shown.
 *
 * There are three states that the UI may flip between; see {@link #DISPLAY_MODE_NORMAL},
 * {@link #DISPLAY_MODE_EXPANDABLE}, and {@link #DISPLAY_MODE_FOCUSED} for details.
 */
public abstract class PaymentRequestSection extends LinearLayout implements View.OnClickListener {
    public static final String TAG = "PaymentRequestUI";

    /** Handles clicks on the widgets and providing data to the PaymentsRequestSection. */
    public interface SectionDelegate extends View.OnClickListener {
        /**
         * Called when the user selects a radio button option from an {@link OptionSection}.
         *
         * @param section Section that was changed.
         * @param option  {@link EditableOption} that was selected.
         */
        void onEditableOptionChanged(PaymentRequestSection section, EditableOption option);

        /** Called when the user clicks the edit icon of the selected EditableOption. */
        void onEditEditableOption(PaymentRequestSection section, EditableOption option);

        /** Called when the user requests adding a new EditableOption to a given section. */
        void onAddEditableOption(PaymentRequestSection section);

        /** Checks whether or not the text should be formatted with a bold label. */
        boolean isBoldLabelNeeded(PaymentRequestSection section);

        /** Checks whether or not the user should be allowed to click on controls. */
        boolean isAcceptingUserInput();

        /** Returns any additional text that needs to be displayed. */
        @Nullable String getAdditionalText(PaymentRequestSection section);

        /** Returns true if the additional text should be stylized as a warning instead of info. */
        boolean isAdditionalTextDisplayingWarning(PaymentRequestSection section);

        /** Called when a section has been clicked. */
        void onSectionClicked(PaymentRequestSection section);
    }

    /** Edit button mode: Hide the button. */
    public static final int EDIT_BUTTON_GONE = 0;

    /** Edit button mode: Indicate that the section requires a selection. */
    public static final int EDIT_BUTTON_CHOOSE = 1;

    /** Edit button mode: Indicate that the section requires adding an option. */
    public static final int EDIT_BUTTON_ADD = 2;

    /** Normal mode: White background, displays the item assuming the user accepts it as is. */
    public static final int DISPLAY_MODE_NORMAL = 3;

    /** Editable mode: White background, displays the item with an edit chevron. */
    public static final int DISPLAY_MODE_EXPANDABLE = 4;

    /** Focused mode: Gray background, more padding, no edit chevron. */
    public static final int DISPLAY_MODE_FOCUSED = 5;

    /** Checking mode: Gray background, spinner overlay hides everything except the title. */
    public static final int DISPLAY_MODE_CHECKING = 6;

    protected final SectionDelegate mDelegate;
    protected final int mLargeSpacing;
    protected final Button mEditButtonView;
    protected final boolean mIsLayoutInitialized;

    protected int mDisplayMode = DISPLAY_MODE_NORMAL;

    private final int mVerticalSpacing;
    private final @ColorInt int mUnfocusedBackgroundColor;
    private final int mFocusedBackgroundColor;
    private final LinearLayout mMainSection;
    private final ImageView mLogoView;
    private final ImageView mChevronView;

    private TextView mTitleView;
    private LinearLayout mSummaryLayout;
    private TextView mSummaryLeftTextView;
    private TextView mSummaryRightTextView;

    private Drawable mLogo;
    private boolean mIsSummaryAllowed = true;

    /**
     * Constructs a PaymentRequestSection.
     *
     * @param context     Context to pull resources from.
     * @param sectionName Title of the section to display.
     * @param delegate    Delegate to alert when something changes in the dialog.
     */
    private PaymentRequestSection(Context context, String sectionName, SectionDelegate delegate) {
        super(context);
        mDelegate = delegate;
        setOnClickListener(delegate);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);

        // Set the styling of the view.
        mUnfocusedBackgroundColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.payment_request_bg);
        mFocusedBackgroundColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.payments_section_edit_background);
        mLargeSpacing =
                getResources().getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing);
        mVerticalSpacing =
                getResources().getDimensionPixelSize(R.dimen.payments_section_vertical_spacing);
        setPadding(mLargeSpacing, mVerticalSpacing, mLargeSpacing, mVerticalSpacing);

        // Create the main content.
        mMainSection = prepareMainSection(sectionName);
        mLogoView = isLogoNecessary() ? createAndAddLogoView(this, mLargeSpacing) : null;
        mEditButtonView = createAndAddEditButton(this);
        mChevronView = createAndAddChevron(this);
        mIsLayoutInitialized = true;
        setDisplayMode(DISPLAY_MODE_NORMAL);
    }

    /**
     * Sets what logo should be displayed.
     *
     * @param logo       The logo to display.
     */
    protected void setLogoDrawable(Drawable logo) {
        assert isLogoNecessary();
        mLogo = logo;
        mLogoView.setBackgroundResource(0);
        mLogoView.setImageDrawable(mLogo);
    }

    /** Returns the LinearLayout containing the summary texts of the section. */
    protected LinearLayout getSummaryLayout() {
        assert mSummaryLayout != null;
        return mSummaryLayout;
    }

    /** Returns the right summary TextView. */
    protected TextView getSummaryRightTextView() {
        assert mSummaryRightTextView != null;
        return mSummaryRightTextView;
    }

    /** Returns the left summary TextView. */
    protected TextView getSummaryLeftTextView() {
        assert mSummaryLeftTextView != null;
        return mSummaryLeftTextView;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        // Allow touches to propagate to children only if the layout can be interacted with.
        return !mDelegate.isAcceptingUserInput();
    }

    @Override
    public final void onClick(View v) {
        if (!mDelegate.isAcceptingUserInput()) return;

        // Handle clicking on "ADD" or "CHOOSE".
        if (v == mEditButtonView) {
            if (getEditButtonState() == EDIT_BUTTON_ADD) {
                mDelegate.onAddEditableOption(this);
            } else {
                mDelegate.onSectionClicked(this);
            }
            return;
        }

        handleClick(v);
        updateControlLayout();
    }

    /** Handles clicks on the PaymentRequestSection. */
    protected void handleClick(View v) { }

    /**
     * Called when the UI is telling the section that it has either gained or lost focus.
     */
    public void focusSection(boolean shouldFocus) {
        setDisplayMode(shouldFocus ? DISPLAY_MODE_FOCUSED : DISPLAY_MODE_EXPANDABLE);
    }

    /**
     * Updates what Views are displayed and how they look.
     *
     * @param displayMode What mode the widget is being displayed in.
     */
    public void setDisplayMode(int displayMode) {
        mDisplayMode = displayMode;
        updateControlLayout();
    }

    /**
     * Changes what is being displayed in the summary.
     *
     * @param leftText  Text to display on the left side.  If null, the whole row hides.
     * @param rightText Text to display on the right side.  If null, only the right View hides.
     */
    public void setSummaryText(
            @Nullable CharSequence leftText, @Nullable CharSequence rightText) {
        mSummaryLeftTextView.setText(leftText);
        mSummaryRightTextView.setText(rightText);
        mSummaryRightTextView.setVisibility(TextUtils.isEmpty(rightText) ? GONE : VISIBLE);
        updateControlLayout();
    }

    /**
     * Changes the appearance of the title.
     *
     * @param resId @see android.widget.TextView#setTextAppearance(int id).
     */
    protected void setTitleAppearance(int resId) {
        ApiCompatibilityUtils.setTextAppearance(mTitleView, resId);
    }

    /**
     * Changes the appearance of the summary.
     *
     * @param resId @see android.widget.TextView#setTextAppearance(int id).
     */
    protected void setSummaryAppearance(int leftResId, int rightResId) {
        ApiCompatibilityUtils.setTextAppearance(mSummaryLeftTextView, leftResId);
        ApiCompatibilityUtils.setTextAppearance(mSummaryRightTextView, rightResId);
    }

    /**
     * Sets how the summary text should be displayed.
     *
     * @param leftTruncate      How to truncate the left summary text.  Set to null to clear.
     * @param leftIsSingleLine  Whether the left summary text should be a single line.
     * @param rightTruncate     How to truncate the right summary text.  Set to null to clear.
     * @param rightIsSingleLine Whether the right summary text should be a single line.
     */
    public void setSummaryProperties(@Nullable TruncateAt leftTruncate, boolean leftIsSingleLine,
            @Nullable TruncateAt rightTruncate, boolean rightIsSingleLine) {
        mSummaryLeftTextView.setEllipsize(leftTruncate);
        mSummaryLeftTextView.setSingleLine(leftIsSingleLine);

        mSummaryRightTextView.setEllipsize(rightTruncate);
        mSummaryRightTextView.setSingleLine(rightIsSingleLine);
    }

    /**
     * Subclasses may override this method to add additional controls to the layout.
     *
     * @param mainSectionLayout Layout containing all of the main content of the section.
     */
    protected abstract void createMainSectionContent(LinearLayout mainSectionLayout);

    /**
     * Sets whether the edit button may be interacted with.
     *
     * @param isEnabled Whether the button may be interacted with.
     */
    public void setIsEditButtonEnabled(boolean isEnabled) {
        mEditButtonView.setEnabled(isEnabled);
    }

    /**
     * Sets whether the summary text can be displayed.
     *
     * @param isAllowed Whether to display the summary text when needed.
     */
    protected void setIsSummaryAllowed(boolean isAllowed) {
        mIsSummaryAllowed = isAllowed;
    }

    /** @return Whether or not the logo should be displayed. */
    protected boolean isLogoNecessary() {
        return false;
    }

    /**
     * Returns the state of the edit button, which is hidden by default.
     *
     * @return State of the edit button.
     */
    public int getEditButtonState() {
        return EDIT_BUTTON_GONE;
    }

    /**
     * Creates the main section.  Subclasses must call super#createMainSection() immediately to
     * guarantee that Views are added in the correct order.
     *
     * @param sectionName Title to display for the section.
     */
    private LinearLayout prepareMainSection(String sectionName) {
        // The main section is a vertical linear layout that subclasses can append to.
        LinearLayout mainSectionLayout = new LinearLayout(getContext());
        mainSectionLayout.setOrientation(VERTICAL);
        LinearLayout.LayoutParams mainParams = new LayoutParams(0, LayoutParams.WRAP_CONTENT);
        mainParams.weight = 1;
        addView(mainSectionLayout, mainParams);

        // The title is always displayed for the row at the top of the main section.
        mTitleView = new TextView(getContext());
        mTitleView.setText(sectionName);
        ApiCompatibilityUtils.setTextAppearance(mTitleView, R.style.TextAppearance_BlueLink2);
        mainSectionLayout.addView(
                mTitleView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        // Create the two TextViews for showing the summary text.
        mSummaryLeftTextView = new TextView(getContext());
        mSummaryLeftTextView.setId(R.id.payments_left_summary_label);
        ApiCompatibilityUtils.setTextAppearance(
                mSummaryLeftTextView, R.style.TextAppearance_BlackTitle1);

        mSummaryRightTextView = new TextView(getContext());
        ApiCompatibilityUtils.setTextAppearance(
                mSummaryRightTextView, R.style.TextAppearance_BlackTitle1);
        mSummaryRightTextView.setTextAlignment(TEXT_ALIGNMENT_TEXT_END);

        // The main TextView sucks up all the available space.
        LinearLayout.LayoutParams leftLayoutParams = new LinearLayout.LayoutParams(
                0, LayoutParams.WRAP_CONTENT);
        leftLayoutParams.weight = 1;

        LinearLayout.LayoutParams rightLayoutParams = new LinearLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        MarginLayoutParamsCompat.setMarginStart(rightLayoutParams,
                getContext().getResources().getDimensionPixelSize(
                        R.dimen.editor_dialog_section_small_spacing));

        // The summary section displays up to two TextViews side by side.
        mSummaryLayout = new LinearLayout(getContext());
        mSummaryLayout.addView(mSummaryLeftTextView, leftLayoutParams);
        mSummaryLayout.addView(mSummaryRightTextView, rightLayoutParams);
        mainSectionLayout.addView(mSummaryLayout, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        setSummaryText(null, null);

        createMainSectionContent(mainSectionLayout);
        return mainSectionLayout;
    }

    private static ImageView createAndAddLogoView(ViewGroup parent, int startMargin) {
        ImageView view = new ImageView(parent.getContext());
        view.setMaxWidth(parent.getContext().getResources().getDimensionPixelSize(
                R.dimen.editable_option_section_logo_width));
        view.setAdjustViewBounds(true);

        // The logo has a pre-defined height and width.
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        MarginLayoutParamsCompat.setMarginStart(params, startMargin);
        parent.addView(view, params);
        return view;
    }

    private Button createAndAddEditButton(ViewGroup parent) {
        Resources resources = parent.getResources();
        Button view = DualControlLayout.createButtonForLayout(
                parent.getContext(), true, resources.getString(R.string.choose), this);
        view.setId(R.id.payments_section);

        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        MarginLayoutParamsCompat.setMarginStart(params, mLargeSpacing);
        parent.addView(view, params);
        return view;
    }

    private ImageView createAndAddChevron(ViewGroup parent) {
        TintedDrawable chevron = TintedDrawable.constructTintedDrawable(parent.getContext(),
                R.drawable.ic_expand_more_black_24dp, R.color.payments_section_chevron);

        ImageView view = new ImageView(parent.getContext());
        view.setImageDrawable(chevron);

        // Wrap whatever image is passed in.
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        MarginLayoutParamsCompat.setMarginStart(params, mLargeSpacing);
        parent.addView(view, params);
        return view;
    }

    /**
     * Called when the section's controls need to be updated after configuration changes.
     *
     * Because of the complicated special casing of what controls hide other controls, all calls to
     * update just one of the controls causes the visibility logic to trigger for all of them.
     *
     * Subclasses should call the super method after they update their own controls.
     */
    protected void updateControlLayout() {
        if (!mIsLayoutInitialized) return;

        boolean isExpanded =
                mDisplayMode == DISPLAY_MODE_FOCUSED || mDisplayMode == DISPLAY_MODE_CHECKING;
        setBackgroundColor(isExpanded ? mFocusedBackgroundColor : mUnfocusedBackgroundColor);

        // Update whether the logo is displayed.
        if (mLogoView != null) {
            boolean show = mLogo != null && mDisplayMode != DISPLAY_MODE_FOCUSED;
            mLogoView.setVisibility(show ? VISIBLE : GONE);
        }

        // The button takes precedence over the summary text and the chevron.
        int editButtonState = getEditButtonState();
        if (editButtonState == EDIT_BUTTON_GONE) {
            mEditButtonView.setVisibility(GONE);
            mChevronView.setVisibility(
                    mDisplayMode == DISPLAY_MODE_EXPANDABLE ? VISIBLE : GONE);
        } else {
            // Show the edit button and hide the chevron.
            boolean isButtonAllowed = mDisplayMode == DISPLAY_MODE_EXPANDABLE
                    || mDisplayMode == DISPLAY_MODE_NORMAL;
            mChevronView.setVisibility(GONE);
            mEditButtonView.setVisibility(isButtonAllowed ? VISIBLE : GONE);
            mEditButtonView.setText(
                    editButtonState == EDIT_BUTTON_CHOOSE ? R.string.choose : R.string.add);
        }

        // Update whether the summary is displayed.
        mSummaryLayout.setVisibility(mIsSummaryAllowed ? VISIBLE : GONE);

        // The title gains extra spacing when there is another visible view in the main section.
        int numVisibleMainViews = 0;
        for (int i = 0; i < mMainSection.getChildCount(); i++) {
            if (mMainSection.getChildAt(i).getVisibility() == VISIBLE) numVisibleMainViews += 1;
        }

        boolean isTitleMarginNecessary = numVisibleMainViews > 1 && isExpanded;
        int oldMargin =
                ((ViewGroup.MarginLayoutParams) mTitleView.getLayoutParams()).bottomMargin;
        int newMargin = isTitleMarginNecessary ? mVerticalSpacing : 0;

        if (oldMargin != newMargin) {
            ((ViewGroup.MarginLayoutParams) mTitleView.getLayoutParams()).bottomMargin =
                    newMargin;
            requestLayout();
        }
    }

    /**
     * Section with an additional Layout for showing a total and how it is broken down.
     *
     * Normal mode:     Just the summary is displayed.
     *                  If no option is selected, the "empty label" is displayed in its place.
     * Expandable mode: Same as Normal, but shows the chevron.
     * Focused mode:    Hides the summary and chevron, then displays the full set of options.
     *
     * ............................................................................
     * . TITLE                                                          |         .
     * .................................................................| CHERVON .
     * . LEFT SUMMARY TEXT          | UPDATE TEXT |  RIGHT SUMMARY TEXT |    or   .
     * .................................................................|   ADD   .
     * .                                      | Line item 1 |    $13.99 |    or   .
     * .                                      | Line item 2 |      $.99 |  CHOOSE .
     * .                                      | Line item 3 |     $2.99 |         .
     * ............................................................................
     */
    public static class LineItemBreakdownSection extends PaymentRequestSection {
        /** The duration of the animation to show and hide the update text. */
        static final int UPDATE_TEXT_ANIMATION_DURATION_MS = 500;

        /** The amount of time where the update text is visible before fading out. */
        static final int UPDATE_TEXT_VISIBILITY_DURATION_MS = 5000;

        /** The GridLayout that shows a breakdown of all the items in the user's card. */
        private GridLayout mBreakdownLayout;

        /**
         * The TextView that is used to display the updated message to the user when the total price
         * of their cart changes. It's the second child of the mSummaryLayout.
         */
        private TextView mUpdatedView;

        private final List<TextView> mLineItemAmountsForTest = new ArrayList<>();

        /** The runnable used to fade out the mUpdatedView. */
        private Runnable mFadeOutRunnable = new Runnable() {
            @Override
            public void run() {
                Animation out = new AlphaAnimation(mUpdatedView.getAlpha(), 0.0f);
                out.setDuration(UPDATE_TEXT_ANIMATION_DURATION_MS);
                out.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
                out.setFillAfter(true);
                mUpdatedView.startAnimation(out);
            }
        };

        /** The Handler used to post the mFadeOutRunnables. */
        private Handler mHandler = new Handler();

        public LineItemBreakdownSection(
                Context context, String sectionName, SectionDelegate delegate, String updatedText) {
            super(context, sectionName, delegate);

            // The mUpdatedView should have been created in the base constructor's call to
            // createMainSectionContent(...).
            assert mUpdatedView != null;
            mUpdatedView.setText(updatedText);
        }

        // This method is called in PaymentRequestSection's constructor.
        @Override
        protected void createMainSectionContent(LinearLayout mainSectionLayout) {
            Context context = mainSectionLayout.getContext();

            // Add a label that will be used to indicate that the total cart price has been updated.
            addUpdateText(mainSectionLayout);

            // The breakdown is represented by an end-aligned GridLayout that takes up only as much
            // space as it needs.  The GridLayout ensures a consistent margin between the columns.
            mBreakdownLayout = new GridLayout(context);
            mBreakdownLayout.setColumnCount(2);
            LayoutParams breakdownParams =
                    new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            breakdownParams.gravity = Gravity.END;
            mainSectionLayout.addView(mBreakdownLayout, breakdownParams);

            // Sets the summary right text view takes the same available space as the summary left
            // text view.
            LinearLayout.LayoutParams rightTextViewLayoutParams =
                    (LinearLayout.LayoutParams) getSummaryRightTextView().getLayoutParams();
            rightTextViewLayoutParams.width = 0;
            rightTextViewLayoutParams.weight = 1f;
        }

        /**
         * Adds a text view to the summary layout that will be used to indicate that the total price
         * of the card been updated. The text to display should be set later in the constructor.
         *
         * @param mainSectionLayout The layout of this section.
         */
        private void addUpdateText(LinearLayout mainSectionLayout) {
            assert mUpdatedView == null;

            Context context = mainSectionLayout.getContext();

            // Create the view and set the text appearance and layout parameters.
            mUpdatedView = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    mUpdatedView, R.style.TextAppearance_BlackTitle1);
            LinearLayout.LayoutParams updatedLayoutParams = new LinearLayout.LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            mUpdatedView.setTextAlignment(TEXT_ALIGNMENT_TEXT_END);
            mUpdatedView.setTextColor(ApiCompatibilityUtils.getColor(
                    context.getResources(), R.color.google_green_600));
            MarginLayoutParamsCompat.setMarginStart(updatedLayoutParams,
                    context.getResources().getDimensionPixelSize(
                            R.dimen.editor_dialog_section_small_spacing));
            MarginLayoutParamsCompat.setMarginEnd(updatedLayoutParams,
                    context.getResources().getDimensionPixelSize(
                            R.dimen.editor_dialog_section_small_spacing));

            // Set the view to initially be invisible.
            mUpdatedView.setVisibility(View.INVISIBLE);

            // Add the update text just before the last summary text.
            getSummaryLayout().addView(
                    mUpdatedView, getSummaryLayout().getChildCount() - 1, updatedLayoutParams);
        }

        /**
         * Updates the total and how it's broken down.
         *
         * @param cart The shopping cart contents and the total.
         */
        public void update(ShoppingCart cart) {
            Context context = mBreakdownLayout.getContext();

            CharSequence totalPrice = createValueString(
                    cart.getTotal().getCurrency(), cart.getTotal().getPrice(), true);

            // Show the updated text view if the total changed.
            showUpdateIfTextChanged(totalPrice);

            // Update the summary to display information about the total.
            setSummaryText(cart.getTotal().getLabel(), totalPrice);

            mBreakdownLayout.removeAllViews();
            mLineItemAmountsForTest.clear();
            if (cart.getContents() == null) return;

            int maximumDescriptionWidthPx =
                    ((View) mBreakdownLayout.getParent()).getWidth() * 2 / 3;

            // Update the breakdown, using one row per {@link LineItem}.
            int numItems = cart.getContents().size();
            mBreakdownLayout.setRowCount(numItems);
            for (int i = 0; i < numItems; i++) {
                LineItem item = cart.getContents().get(i);

                TextView description = new TextView(context);
                ApiCompatibilityUtils.setTextAppearance(description,
                        item.getIsPending()
                            ? R.style.TextAppearance_PaymentsUiSectionPendingTextEndAligned
                            : R.style.TextAppearance_PaymentsUiSectionDescriptiveTextEndAligned);
                description.setText(item.getLabel());
                description.setEllipsize(TruncateAt.END);
                description.setMaxLines(2);
                if (maximumDescriptionWidthPx > 0) {
                    description.setMaxWidth(maximumDescriptionWidthPx);
                }

                TextView amount = new TextView(context);
                ApiCompatibilityUtils.setTextAppearance(amount,
                        item.getIsPending()
                            ? R.style.TextAppearance_PaymentsUiSectionPendingTextEndAligned
                            : R.style.TextAppearance_PaymentsUiSectionDescriptiveTextEndAligned);
                amount.setText(createValueString(item.getCurrency(), item.getPrice(), false));
                mLineItemAmountsForTest.add(amount);

                // Each item is represented by a row in the GridLayout.
                GridLayout.LayoutParams descriptionParams = new GridLayout.LayoutParams(
                        GridLayout.spec(i, 1, GridLayout.END),
                        GridLayout.spec(0, 1, GridLayout.END));
                GridLayout.LayoutParams amountParams = new GridLayout.LayoutParams(
                        GridLayout.spec(i, 1, GridLayout.END),
                        GridLayout.spec(1, 1, GridLayout.END));
                MarginLayoutParamsCompat.setMarginStart(amountParams,
                        context.getResources().getDimensionPixelSize(
                                R.dimen.payments_section_descriptive_item_spacing));

                mBreakdownLayout.addView(description, descriptionParams);
                mBreakdownLayout.addView(amount, amountParams);
            }
        }

        /**
         * Show the update text if the cart total has changed. Should be called before changing the
         * cart total because the old total is needed for comparison.
         *
         * @param rightText The new cart total that will replace the one currently displayed.
         */
        private void showUpdateIfTextChanged(@Nullable CharSequence rightText) {
            // If either the old or new text was null do nothing.
            if (rightText == null || getSummaryRightTextView().getText() == null) return;

            // Show the update text only if the current and new cart totals are different and if the
            // old total was visible to the user.
            if (!TextUtils.equals(getSummaryRightTextView().getText(), rightText)
                    && getSummaryRightTextView().getVisibility() == VISIBLE) {
                startUpdateViewAnimation();
            }
        }

        /**
         * Starts the animation to make the update text view fade in then fade out.
         */
        private void startUpdateViewAnimation() {
            // Create and start a fade in anmiation for the mUpdatedView. Re-use the current alpha
            // to avoid restarting a previous or current fade in animation.
            Animation in = new AlphaAnimation(mUpdatedView.getAlpha(), 1.0f);
            in.setDuration(UPDATE_TEXT_ANIMATION_DURATION_MS);
            in.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            in.setFillAfter(true);
            mUpdatedView.startAnimation(in);

            // Cancel all pending fade out animations and create a new on to be executed a little
            // while after the fade in.
            mHandler.removeCallbacks(mFadeOutRunnable);
            mHandler.postDelayed(mFadeOutRunnable, UPDATE_TEXT_VISIBILITY_DURATION_MS);
        }

        /**
         * Builds a CharSequence that displays a value in a particular currency.
         *
         * @param currency    Currency of the value being displayed.
         * @param value       Value to display.
         * @param isValueBold Whether or not to bold the item.
         * @return CharSequence that represents the whole value.
         */
        private CharSequence createValueString(String currency, String value, boolean isValueBold) {
            SpannableStringBuilder valueBuilder = new SpannableStringBuilder();
            valueBuilder.append(currency);
            valueBuilder.append(" ");

            int boldStartIndex = valueBuilder.length();
            valueBuilder.append(value);

            if (isValueBold) {
                valueBuilder.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), boldStartIndex,
                        boldStartIndex + value.length(), 0);
            }

            return valueBuilder;
        }

        @Override
        public void setDisplayMode(int displayMode) {
            // Displays the summary left text view in at most three lines if in focus mode,
            // otherwise display it in a single line.
            if (displayMode == DISPLAY_MODE_FOCUSED) {
                setSummaryProperties(TruncateAt.END, false /* leftIsSingleLine */,
                        null /* rightTruncate */, false /* rightIsSingleLine */);
                getSummaryLeftTextView().setMaxLines(3);
            } else {
                setSummaryProperties(TruncateAt.END, true /* leftIsSingleLine */,
                        null /* rightTruncate */, false /* rightIsSingleLine */);
                getSummaryLeftTextView().setMaxLines(1);
            }

            super.setDisplayMode(displayMode);
        }

        @Override
        protected void updateControlLayout() {
            if (!mIsLayoutInitialized) return;

            mBreakdownLayout.setVisibility(mDisplayMode == DISPLAY_MODE_FOCUSED ? VISIBLE : GONE);
            super.updateControlLayout();
        }

        /**
         * Returns the line item amount at the specified |index|. Returns null if there is no amount
         * at that index.
         */
        @VisibleForTesting
        public TextView getLineItemAmountForTest(int index) {
            return mLineItemAmountsForTest.get(index);
        }

        /** @return The number of line items. */
        @VisibleForTesting
        public int getNumberOfLineItemsForTest() {
            return mLineItemAmountsForTest.size();
        }
    }

    /**
     * Section that allows selecting one thing from a set of mutually-exclusive options.
     *
     * Normal mode:     The summary text displays the selected option, and the icon for the option
     *                  is displayed in the logo section (if it exists).
     *                  If no option is selected, the "empty label" is displayed in its place.
     *                  This is important for shipping options (e.g.) because there will be no
     *                  option selected by default and a prompt can be displayed.
     * Expandable mode: Same as Normal, but shows the chevron.
     * Focused mode:    Hides the summary and chevron, then displays the full set of options.
     *
     * .............................................................................................
     * . TITLE                                                          |                |         .
     * .................................................................|                |         .
     * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT |                |         .
     * .................................................................|                | CHEVRON .
     * . Descriptive text that spans all three columns because it can.  |                |    or   .
     * . ! Warning text that displays a big scary warning and icon.     |           LOGO |   ADD   .
     * . O Option 1                                  ICON 1 | Edit Icon |                |    or   .
     * . O Option 2                                  ICON 2 | Edit Icon |                |  CHOOSE .
     * . O Option 3                                  ICON 3 | Edit Icon |                |         .
     * . + ADD THING                                                    |                |         .
     * .............................................................................................
     */
    public static class OptionSection extends PaymentRequestSection {

        private static final int INVALID_OPTION_INDEX = -1;

        private final List<TextView> mLabelsForTest = new ArrayList<>();
        private boolean mCanAddItems = true;

        /**
         * Observer to be notified when the OptionSection changes focus state.
         */
        public interface FocusChangedObserver {
            /*
             * Called when the OptionSection view gets or loses focus.
             *
             * @param dataType  The type of the data contained in the section.
             * @param willFocus Whether the section is getting the focus.
             */
            void onFocusChanged(@PaymentRequestUI.DataType int dataType, boolean willFocus);
        }

        /**
         * Displays a row representing either a selectable option or some flavor text.
         *
         * + The "button" is on the left and shows either an icon or a radio button to represent th
         *   row type.
         * + The "label" is text describing the row.
         * + The "icon" is a logo representing the option, like a credit card.
         * + The "edit icon" is a pencil icon with a vertical separator to indicate the option is
         *   editable, clicking on it brings up corresponding editor.
         */
        public class OptionRow {
            private static final int OPTION_ROW_TYPE_OPTION = 0;
            private static final int OPTION_ROW_TYPE_ADD = 1;
            private static final int OPTION_ROW_TYPE_DESCRIPTION = 2;
            private static final int OPTION_ROW_TYPE_WARNING = 3;

            private final int mRowType;
            @Nullable
            private final EditableOption mOption;
            private final View mButton;
            private final TextView mLabel;
            private final View mOptionIcon;
            private final View mEditIcon;

            public OptionRow(GridLayout parent, int rowIndex, int rowType,
                    @Nullable EditableOption item, boolean isSelected) {
                assert item != null || rowType != OPTION_ROW_TYPE_OPTION;
                boolean optionIconExists = item != null && item.getDrawableIcon() != null;
                boolean editIconExists = item != null && item.isEditable() && isSelected;
                boolean isEnabled = item != null && item.isValid();
                mRowType = rowType;
                mOption = item;
                mButton = createButton(parent, rowIndex, isSelected, isEnabled);
                mLabel = createLabel(parent, rowIndex, optionIconExists, editIconExists, isEnabled);
                mOptionIcon = optionIconExists
                        ? createOptionIcon(parent, rowIndex, editIconExists) : null;
                mEditIcon = editIconExists ? createEditIcon(parent, rowIndex) : null;
            }

            /** Sets the selected state of this item, alerting the delegate if selected. */
            public void setChecked(boolean isChecked) {
                if (mOption == null) return;

                ((RadioButton) mButton).setChecked(isChecked);
                if (isChecked) {
                    updateSelectedItem(mOption);
                    mDelegate.onEditableOptionChanged(OptionSection.this, mOption);
                }
            }

            /** Returns whether this OptionRow's RadioButton is checked. */
            public boolean isChecked() {
                return ((RadioButton) mButton).isChecked();
            }

            /** Change the label for the row. */
            public void setLabel(int stringId) {
                setLabel(getContext().getString(stringId));
            }

            /** Change the label for the row. */
            public void setLabel(CharSequence string) {
                mLabel.setText(string);
            }

            /** Set the button identifier for the option. */
            public void setButtonId(int id) {
                mButton.setId(id);
            }

            /** @return the label for the row. */
            @VisibleForTesting
            public CharSequence getLabelText() {
                return mLabel.getText();
            }

            private View createButton(
                    GridLayout parent, int rowIndex, boolean isSelected, boolean isEnabled) {
                if (mRowType == OPTION_ROW_TYPE_DESCRIPTION) return null;

                Context context = parent.getContext();
                View view;

                if (mRowType == OPTION_ROW_TYPE_OPTION) {
                    // Show a radio button indicating whether the EditableOption is selected.
                    RadioButton button = new RadioButton(context);
                    button.setChecked(isSelected && isEnabled);
                    button.setEnabled(isEnabled);
                    view = button;
                } else {
                    // Show an icon representing the row type, defaulting to the add button.
                    int drawableId;
                    int drawableTint;
                    if (mRowType == OPTION_ROW_TYPE_WARNING) {
                        drawableId = R.drawable.ic_warning_white_24dp;
                        drawableTint = R.color.error_text_color;
                    } else {
                        drawableId = R.drawable.plus;
                        drawableTint = R.color.default_icon_color_blue;
                    }

                    TintedDrawable tintedDrawable = TintedDrawable.constructTintedDrawable(
                            context, drawableId, drawableTint);
                    ImageButton button = new ImageButton(context);
                    button.setBackground(null);
                    button.setImageDrawable(tintedDrawable);
                    button.setPadding(0, 0, 0, 0);
                    view = button;
                }

                // The button hugs left.
                GridLayout.LayoutParams buttonParams = new GridLayout.LayoutParams(
                        GridLayout.spec(rowIndex, 1, GridLayout.CENTER),
                        GridLayout.spec(0, 1, GridLayout.CENTER));
                buttonParams.topMargin = mVerticalMargin;
                MarginLayoutParamsCompat.setMarginEnd(buttonParams, mLargeSpacing);
                parent.addView(view, buttonParams);

                view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
                view.setOnClickListener(OptionSection.this);
                return view;
            }

            private TextView createLabel(GridLayout parent, int rowIndex, boolean optionIconExists,
                    boolean editIconExists, boolean isEnabled) {
                Context context = parent.getContext();
                Resources resources = context.getResources();

                // By default, the label appears to the right of the "button" in the second column.
                // + If there is no button, no option and edit icon, the label spans the whole row.
                // + If there is no option and edit icon, the label spans three columns.
                // + If there is no edit icon or option icon, the label spans two columns.
                // + Otherwise, the label occupies only its own column.
                int columnStart = 1;
                int columnSpan = 1;
                if (!optionIconExists) columnSpan++;
                if (!editIconExists) columnSpan++;

                TextView labelView = new TextView(context);
                if (mRowType == OPTION_ROW_TYPE_OPTION) {
                    // Show the string representing the EditableOption.
                    labelView.setText(convertOptionToString(mOption, false, /* excludeMainLabel */
                            mDelegate.isBoldLabelNeeded(OptionSection.this),
                            false /* singleLine */));
                    labelView.setEnabled(isEnabled);
                } else if (mRowType == OPTION_ROW_TYPE_ADD) {
                    // Shows string saying that the user can add a new option, e.g. credit card no.
                    int buttonHeight = resources.getDimensionPixelSize(
                            R.dimen.payments_section_add_button_height);

                    ApiCompatibilityUtils.setTextAppearance(
                            labelView, R.style.TextAppearance_EditorDialogSectionAddButton);
                    labelView.setMinimumHeight(buttonHeight);
                    labelView.setGravity(Gravity.CENTER_VERTICAL);
                    labelView.setTypeface(UiUtils.createRobotoMediumTypeface());
                } else if (mRowType == OPTION_ROW_TYPE_DESCRIPTION) {
                    // The description spans all the columns.
                    columnStart = 0;
                    columnSpan = 4;

                    ApiCompatibilityUtils.setTextAppearance(
                            labelView, R.style.TextAppearance_BlackBody);
                    labelView.setId(R.id.payments_description_label);
                } else if (mRowType == OPTION_ROW_TYPE_WARNING) {
                    // Warnings use three columns.
                    columnSpan = 3;
                    ApiCompatibilityUtils.setTextAppearance(
                            labelView, R.style.TextAppearance_PaymentsUiSectionWarningText);
                    labelView.setId(R.id.payments_warning_label);
                }

                // The label spans two columns if no option or edit icon, or spans three columns if
                // no option and edit icons. Setting the view width to 0 forces it to stretch.
                GridLayout.LayoutParams labelParams =
                        new GridLayout.LayoutParams(GridLayout.spec(rowIndex, 1, GridLayout.CENTER),
                                GridLayout.spec(columnStart, columnSpan, GridLayout.FILL, 1f));
                labelParams.topMargin = mVerticalMargin;
                labelParams.width = 0;
                if (optionIconExists) {
                    // Margin at the end of the label instead of the start of the option icon to
                    // allow option icon in the the next row align with the end of label (include
                    // end margin) when edit icon exits in that row, like below:
                    // ---Label---------------------[label margin]|---option icon---|
                    // ---Label---[label margin]|---option icon---|----edit icon----|
                    MarginLayoutParamsCompat.setMarginEnd(labelParams, mLargeSpacing);
                }
                parent.addView(labelView, labelParams);

                labelView.setOnClickListener(OptionSection.this);
                return labelView;
            }

            private View createOptionIcon(GridLayout parent, int rowIndex, boolean editIconExists) {
                // The icon has a pre-defined width.
                ImageView optionIcon = new ImageView(parent.getContext());
                optionIcon.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
                if (mOption.isEditable()) {
                    optionIcon.setMaxWidth(mEditableOptionIconMaxWidth);
                } else {
                    optionIcon.setMaxWidth(mNonEditableOptionIconMaxWidth);
                }
                optionIcon.setAdjustViewBounds(true);
                optionIcon.setImageDrawable(mOption.getDrawableIcon());

                // Place option icon at column three if no edit icon.
                int columnStart = editIconExists ? 2 : 3;
                GridLayout.LayoutParams iconParams =
                        new GridLayout.LayoutParams(GridLayout.spec(rowIndex, 1, GridLayout.CENTER),
                                GridLayout.spec(columnStart, 1, GridLayout.CENTER));
                iconParams.topMargin = mVerticalMargin;
                parent.addView(optionIcon, iconParams);

                optionIcon.setOnClickListener(OptionSection.this);
                return optionIcon;
            }

            private View createEditIcon(GridLayout parent, int rowIndex) {
                View editorIcon = LayoutInflater.from(parent.getContext())
                                          .inflate(R.layout.payment_option_edit_icon, null);

                // The icon floats to the right of everything.
                GridLayout.LayoutParams iconParams =
                        new GridLayout.LayoutParams(GridLayout.spec(rowIndex, 1, GridLayout.CENTER),
                                GridLayout.spec(3, 1, GridLayout.CENTER));
                iconParams.topMargin = mVerticalMargin;
                parent.addView(editorIcon, iconParams);

                editorIcon.setOnClickListener(OptionSection.this);
                return editorIcon;
            }

            /** Returns the edit icon for the option row. */
            @VisibleForTesting
            public View getEditIconForTest() {
                return mEditIcon;
            }
        }

        /** Top and bottom margins for each item. */
        private final int mVerticalMargin;

        /** All the possible EditableOptions in Layout form, then one row for adding new options. */
        private final ArrayList<OptionRow> mOptionRows = new ArrayList<>();

        /** Width that the editable option icon takes. */
        private final int mEditableOptionIconMaxWidth;

        /** Width that the non editable option icon takes. */
        private final int mNonEditableOptionIconMaxWidth;

        /** Layout containing all the {@link OptionRow}s. */
        private GridLayout mOptionLayout;

        /** A spinner to show when the user selection is being checked. */
        private View mCheckingProgress;

        /** SectionInformation that is used to populate the views in this section. */
        private SectionInformation mSectionInformation;

        /** Indicates whether the summary is displayed in a single line. */
        private boolean mSummaryInSingleLine;

        /**
         * Indicates whether the summary is set to display in a single line in DISPLAY_MODE_NORMAL
         * by caller.
         */
        private boolean mSetDisplaySummaryInSingleLineInNormalMode = true;

        /**
         * Indicates whether the summary should be split to display in left and right summary
         * text views in {@link DISPLAY_MODE_NORMAL}.
         */
        private boolean mSplitSummaryInDisplayModeNormal;

        /** Indicates whether the summary is set to descriptive or title text style. */
        private boolean mSummaryInDescriptiveText;

        private FocusChangedObserver mFocusChangedObserver;

        /**
         * Constructs an OptionSection.
         *
         * @param context     Context to pull resources from.
         * @param sectionName Title of the section to display.
         * @param delegate    Delegate to alert when something changes in the dialog.
         */
        public OptionSection(Context context, String sectionName, SectionDelegate delegate) {
            super(context, sectionName, delegate);
            mVerticalMargin = context.getResources().getDimensionPixelSize(
                    R.dimen.editor_dialog_section_small_spacing);
            mEditableOptionIconMaxWidth = context.getResources().getDimensionPixelSize(
                    R.dimen.editable_option_section_logo_width);
            mNonEditableOptionIconMaxWidth =
                    context.getResources().getDimensionPixelSize(R.dimen.payments_favicon_size);
            setSummaryText(null, null);
        }

        /**
         * Registers the delegate to be notified when this OptionSection gains or loses focus.
         *
         * @param delegate The delegate to notify.
         */
        public void setOptionSectionFocusChangedObserver(FocusChangedObserver observer) {
            mFocusChangedObserver = observer;
        }

        @Override
        public void handleClick(View v) {
            for (int i = 0; i < mOptionRows.size(); i++) {
                OptionRow row = mOptionRows.get(i);
                boolean clickedSelect = row.mButton == v || row.mLabel == v || row.mOptionIcon == v;
                // Handle click on the "ADD THING" button.
                if (row.mOption == null && clickedSelect) {
                    mDelegate.onAddEditableOption(this);
                    return;
                }

                // Handle click on the edit icon.
                if (row.mOption != null && row.mEditIcon == v) {
                    mDelegate.onEditEditableOption(this, row.mOption);
                    return;
                }
            }

            // Update the radio button state: checked/unchecked.
            for (int i = 0; i < mOptionRows.size(); i++) {
                OptionRow row = mOptionRows.get(i);
                boolean clickedSelect = row.mButton == v || row.mLabel == v || row.mOptionIcon == v;
                if (row.mOption != null) row.setChecked(clickedSelect);
            }
        }

        @Override
        public void focusSection(boolean shouldFocus) {
            // Override expansion of the section if there's no options to show.
            boolean mayFocus = mSectionInformation != null && mSectionInformation.getSize() > 0;
            if (!mayFocus && shouldFocus) {
                setDisplayMode(PaymentRequestSection.DISPLAY_MODE_NORMAL);
                return;
            }

            // Notify the observer that the focus is going to change.
            if (mFocusChangedObserver != null) {
                mFocusChangedObserver.onFocusChanged(
                        mSectionInformation.getDataType(), shouldFocus);
            }

            int previousDisplayMode = mDisplayMode;
            super.focusSection(shouldFocus);

            // Update summary when display mode changed from DISPLAY_MODE_NORMAL to other modes.
            if (mSectionInformation != null && previousDisplayMode == DISPLAY_MODE_NORMAL) {
                updateSelectedItem(mSectionInformation.getSelectedItem());
            }
        }

        @Override
        protected boolean isLogoNecessary() {
            return true;
        }

        @Override
        protected void createMainSectionContent(LinearLayout mainSectionLayout) {
            Context context = mainSectionLayout.getContext();
            mCheckingProgress = createLoadingSpinner();

            mOptionLayout = new GridLayout(context);
            mOptionLayout.setColumnCount(4);
            mainSectionLayout.addView(mOptionLayout, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        /** @param canAddItems If false, this section will not show [+ ADD THING] button. */
        public void setCanAddItems(boolean canAddItems) {
            mCanAddItems = canAddItems;
        }

        /**
         * @param singleLine If true, sets the summary text to display in a single line
         *                   in {@link #DISPLAY_MODE_NORMAL} when there is a valid selected
         *                   option, otherwise sets the summary text to display in multiple lines.
         */
        public void setDisplaySummaryInSingleLineInNormalMode(boolean singleLine) {
            mSetDisplaySummaryInSingleLineInNormalMode = singleLine;
        }

        /**
         * Specify whether the summary should be split to display under DISPLAY_MODE_NORMAL.
         *
         * @param splitSummary If true split the display of summary in the left and right
         *                     text views in {@link DISPLAY_MODE_NORMAL}, the summary is
         *                     split into 'label' and the rest('sublabel', 'Tertiary label').
         *                     Otherwise the entire summary is displayed in the left text view.
         */
        public void setSplitSummaryInDisplayModeNormal(boolean splitSummary) {
            mSplitSummaryInDisplayModeNormal = splitSummary;
        }

        /** Updates the View to account for the new {@link SectionInformation} being passed in. */
        public void update(SectionInformation information) {
            mSectionInformation = information;
            EditableOption selectedItem = information.getSelectedItem();
            updateSelectedItem(selectedItem);
            updateOptionList(information, selectedItem);
            updateControlLayout();
        }

        private View createLoadingSpinner() {
            ViewGroup spinnyLayout = (ViewGroup) LayoutInflater.from(getContext()).inflate(
                    R.layout.payment_request_spinny, null);

            TextView textView = (TextView) spinnyLayout.findViewById(R.id.message);
            textView.setText(getContext().getString(R.string.payments_checking_option));

            return spinnyLayout;
        }

        private void setSpinnerVisibility(boolean visibility) {
            if (visibility) {
                if (mCheckingProgress.getParent() != null) return;

                ViewGroup parent = (ViewGroup) mOptionLayout.getParent();
                int optionLayoutIndex = parent.indexOfChild(mOptionLayout);
                parent.addView(mCheckingProgress, optionLayoutIndex);

                MarginLayoutParams params =
                        (MarginLayoutParams) mCheckingProgress.getLayoutParams();
                params.width = LayoutParams.MATCH_PARENT;
                params.height = LayoutParams.WRAP_CONTENT;
                params.bottomMargin = getContext().getResources().getDimensionPixelSize(
                        R.dimen.payments_section_checking_spacing);
                mCheckingProgress.requestLayout();
            } else {
                if (mCheckingProgress.getParent() == null) return;

                ViewGroup parent = (ViewGroup) mCheckingProgress.getParent();
                parent.removeView(mCheckingProgress);
            }
        }

        @Override
        protected void updateControlLayout() {
            if (!mIsLayoutInitialized) return;

            if (mDisplayMode == DISPLAY_MODE_FOCUSED) {
                setIsSummaryAllowed(false);
                mOptionLayout.setVisibility(VISIBLE);
                setSpinnerVisibility(false);
            } else if (mDisplayMode == DISPLAY_MODE_CHECKING) {
                setIsSummaryAllowed(false);
                mOptionLayout.setVisibility(GONE);
                setSpinnerVisibility(true);
            } else {
                setIsSummaryAllowed(true);
                mOptionLayout.setVisibility(GONE);
                setSpinnerVisibility(false);
            }

            super.updateControlLayout();
        }

        @Override
        public int getEditButtonState() {
            if (mSectionInformation == null) return EDIT_BUTTON_GONE;

            if (mSectionInformation.getSize() == 0 && mCanAddItems) {
                // There aren't any EditableOptions.  Ask the user to add a new one.
                return EDIT_BUTTON_ADD;
            } else if (mSectionInformation.getSelectedItem() == null) {
                // The user hasn't selected any available EditableOptions.  Ask the user to pick
                // one.
                return EDIT_BUTTON_CHOOSE;
            } else {
                return EDIT_BUTTON_GONE;
            }
        }

        private void updateSelectedItem(EditableOption selectedItem) {
            // Only left TextView in the summary section is used in this section.
            // Summary is displayed in multiple lines by default unless:
            // 1. nothing is selected or
            // 2. the display mode is DISPLAY_MODE_NORMAL without caller explicitly set to display
            //    summary in multiple lines.
            if (selectedItem == null
                    || (mDisplayMode == DISPLAY_MODE_NORMAL
                               && mSetDisplaySummaryInSingleLineInNormalMode)) {
                if (!mSummaryInSingleLine) {
                    setSummaryProperties(TruncateAt.END, true /* leftIsSingleLine */,
                            null /* rightTruncate */, false /* rightIsSingleLine */);
                    mSummaryInSingleLine = true;
                }
            } else if (mSummaryInSingleLine) {
                setSummaryProperties(null /* leftTruncate */, false /* leftIsSingleLine */,
                        null /* rightTruncate */, false /* rightIsSingleLine */);
                mSummaryInSingleLine = false;
            }

            if (selectedItem == null) {
                setLogoDrawable(null);
                // Section summary should be displayed as descriptive text style.
                if (!mSummaryInDescriptiveText) {
                    ApiCompatibilityUtils.setTextAppearance(
                            getSummaryLeftTextView(), R.style.TextAppearance_BlackBody);
                    mSummaryInDescriptiveText = true;
                }
                SectionUiUtils.showSectionSummaryInTextViewInSingeLine(
                        getContext(), mSectionInformation, getSummaryLeftTextView());
            } else {
                setLogoDrawable(selectedItem.getDrawableIcon());
                // Selected item summary should be displayed as R.style.TextAppearance_BlackTitle1.
                if (mSummaryInDescriptiveText) {
                    ApiCompatibilityUtils.setTextAppearance(
                            getSummaryLeftTextView(), R.style.TextAppearance_BlackTitle1);
                    mSummaryInDescriptiveText = false;
                }
                // Split summary in DISPLAY_MODE_NORMAL if caller specified. The first part is
                // displayed on the left summary text view aligned to the left. The second part is
                // displayed on the right summary text view aligned to the right.
                boolean splitSummary =
                        mSplitSummaryInDisplayModeNormal && (mDisplayMode == DISPLAY_MODE_NORMAL);
                if (splitSummary) {
                    setSummaryText(selectedItem.getLabel(),
                            convertOptionToString(selectedItem, true /* excludeMainLabel */,
                                    false /* useBoldLabel */, mSummaryInSingleLine));
                } else {
                    setSummaryText(convertOptionToString(selectedItem, false /* excludeMainLabel */,
                                           false /* useBoldLabel */, mSummaryInSingleLine),
                            null);
                }
            }

            updateControlLayout();
        }

        private void updateOptionList(SectionInformation information, EditableOption selectedItem) {
            mOptionLayout.removeAllViews();
            mOptionRows.clear();
            mLabelsForTest.clear();

            // Show any additional text requested by the layout.
            String additionalText = mDelegate.getAdditionalText(this);
            if (!TextUtils.isEmpty(additionalText)) {
                OptionRow descriptionRow = new OptionRow(mOptionLayout,
                        mOptionRows.size(),
                        mDelegate.isAdditionalTextDisplayingWarning(this)
                                ? OptionRow.OPTION_ROW_TYPE_WARNING
                                : OptionRow.OPTION_ROW_TYPE_DESCRIPTION,
                                null, false);
                mOptionRows.add(descriptionRow);
                descriptionRow.setLabel(additionalText);
            }

            // List out known payment options.
            int firstOptionIndex = INVALID_OPTION_INDEX;
            for (int i = 0; i < information.getSize(); i++) {
                int currentRow = mOptionRows.size();
                if (firstOptionIndex == INVALID_OPTION_INDEX) firstOptionIndex = currentRow;

                EditableOption item = information.getItem(i);
                OptionRow currentOptionRow = new OptionRow(mOptionLayout, currentRow,
                        OptionRow.OPTION_ROW_TYPE_OPTION, item, item == selectedItem);
                mOptionRows.add(currentOptionRow);

                // For testing, keep the labels in a list for easy access.
                mLabelsForTest.add(currentOptionRow.mLabel);
            }

            // TODO(crbug.com/627186): Find another way to give access to this resource in tests.
            // For testing.
            if (firstOptionIndex != INVALID_OPTION_INDEX) {
                mOptionRows.get(firstOptionIndex).setButtonId(R.id.payments_first_radio_button);
            }

            // If the user is allowed to add new options, show the button for it.
            if (information.getAddStringId() != 0 && mCanAddItems) {
                OptionRow addRow = new OptionRow(mOptionLayout, mOptionLayout.getChildCount(),
                        OptionRow.OPTION_ROW_TYPE_ADD, null, false);
                addRow.setLabel(information.getAddStringId());
                addRow.setButtonId(R.id.payments_add_option_button);
                mOptionRows.add(addRow);
            }
        }

        private CharSequence convertOptionToString(EditableOption item, boolean excludeMainLabel,
                boolean useBoldLabel, boolean singleLine) {
            SpannableStringBuilder builder = new SpannableStringBuilder();
            if (!excludeMainLabel) {
                builder.append(item.getLabel());
                if (useBoldLabel) {
                    builder.setSpan(
                            new StyleSpan(android.graphics.Typeface.BOLD), 0, builder.length(), 0);
                }
            }

            String labelSeparator = singleLine
                    ? getContext().getString(R.string.autofill_address_summary_separator)
                    : "\n";
            if (!TextUtils.isEmpty(item.getSublabel())) {
                if (builder.length() > 0) builder.append(labelSeparator);
                builder.append(item.getSublabel());
            }

            if (!TextUtils.isEmpty(item.getTertiaryLabel())) {
                if (builder.length() > 0) builder.append(labelSeparator);
                builder.append(item.getTertiaryLabel());
            }

            if (!TextUtils.isEmpty(item.getPromoMessage())) {
                if (builder.length() > 0) builder.append(labelSeparator);
                builder.append(item.getPromoMessage());
            }

            if (!item.isComplete() && !TextUtils.isEmpty(item.getEditMessage())) {
                if (builder.length() > 0) builder.append(labelSeparator);
                String editMessage = item.getEditMessage();
                builder.append(editMessage);
                Object foregroundSpanner = new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                        getContext().getResources(), R.color.default_text_color_link));
                Object sizeSpanner = new AbsoluteSizeSpan(14, true);
                int startIndex = builder.length() - editMessage.length();
                builder.setSpan(foregroundSpanner, startIndex, builder.length(), 0);
                builder.setSpan(sizeSpanner, startIndex, builder.length(), 0);
            }

            return builder;
        }

        /**
         * Returns the label at the specified |labelIndex|. Returns null if there is no label at
         * that index.
         */
        @VisibleForTesting
        public TextView getOptionLabelsForTest(int labelIndex) {
            return mLabelsForTest.get(labelIndex);
        }

        /**
         * Returns the label of the section summary.
         */
        @VisibleForTesting
        public TextView getLeftSummaryLabelForTest() {
            return getSummaryLeftTextView();
        }

        /**
         * Returns the right summary text view.
         */
        @VisibleForTesting
        public TextView getRightSummaryLabelForTest() {
            return getSummaryRightTextView();
        }

        /** Returns the number of option labels. */
        @VisibleForTesting
        public int getNumberOfOptionLabelsForTest() {
            return mLabelsForTest.size();
        }

        /** Returns the OptionRow at the specified |index|. */
        @VisibleForTesting
        public OptionRow getOptionRowAtIndex(int index) {
            return mOptionRows.get(index);
        }
    }

    /**
     * Drawn as a 1dp separator.  Initially drawn without being expanded to the full width of the
     * UI, but can be expanded to separate sections fully.
     */
    public static class SectionSeparator extends View {
        /** Creates the View and adds it to the parent. */
        public SectionSeparator(ViewGroup parent) {
            this(parent, -1);
        }

        /** Creates the View and adds it to the parent at the given index. */
        public SectionSeparator(ViewGroup parent, int index) {
            super(parent.getContext());
            Resources resources = parent.getContext().getResources();
            setBackground(HorizontalListDividerDrawable.create(getContext()));
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                            resources.getDimensionPixelSize(R.dimen.divider_height));

            int margin =
                    resources.getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing);
            MarginLayoutParamsCompat.setMarginStart(params, margin);
            MarginLayoutParamsCompat.setMarginEnd(params, margin);
            parent.addView(this, index, params);
        }

        /** Expand the separator to be the full width of the dialog. */
        public void expand() {
            LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) getLayoutParams();
            MarginLayoutParamsCompat.setMarginStart(params, 0);
            MarginLayoutParamsCompat.setMarginEnd(params, 0);
        }
    }
}
