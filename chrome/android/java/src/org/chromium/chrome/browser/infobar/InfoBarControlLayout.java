// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Paint;
import android.support.v7.widget.SwitchCompat;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.DualControlLayout;
import org.chromium.chrome.browser.ui.widget.RadioButtonLayout;

import java.util.List;

/**
 * Lays out a group of controls (e.g. switches, spinners, or additional text) for InfoBars that need
 * more than the normal pair of buttons.
 *
 * This class works with the {@link InfoBarLayout} to define a standard set of controls with
 * standardized spacings and text styling that gets laid out in grid form: https://crbug.com/543205
 *
 * Manually specified margins on the children managed by this layout are EXPLICITLY ignored to
 * enforce a uniform margin between controls across all InfoBar types.  Do NOT circumvent this
 * restriction with creative layout definitions.  If the layout algorithm doesn't work for your new
 * InfoBar, convince Chrome for Android's UX team to amend the master spec and then change the
 * layout algorithm to match.
 *
 * TODO(dfalcantara): The line spacing multiplier is applied to all lines in JB & KK, even if the
 *                    TextView has only one line.  This throws off vertical alignment.  Find a
 *                    solution that hopefully doesn't involve subclassing the TextView.
 */
public final class InfoBarControlLayout extends ViewGroup {
    /**
     * ArrayAdapter that automatically determines what size make its Views to accommodate all of
     * its potential values.
     * @param <T> Type of object that the ArrayAdapter stores.
     */
    public static final class InfoBarArrayAdapter<T> extends ArrayAdapter<T> {
        private final String mLabel;
        private int mMinWidthRequiredForValues;

        public InfoBarArrayAdapter(Context context, String label) {
            super(context, R.layout.infobar_control_spinner_drop_down);
            mLabel = label;
        }

        public InfoBarArrayAdapter(Context context, T[] objects) {
            super(context, R.layout.infobar_control_spinner_drop_down, objects);
            mLabel = null;
        }

        @Override
        public View getDropDownView(int position, View convertView, ViewGroup parent) {
            TextView view;
            if (convertView instanceof TextView) {
                view = (TextView) convertView;
            } else {
                view = (TextView) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_control_spinner_drop_down, parent, false);
            }

            view.setText(getItem(position).toString());
            return view;
        }

        @Override
        public DualControlLayout getView(int position, View convertView, ViewGroup parent) {
            DualControlLayout view;
            if (convertView instanceof DualControlLayout) {
                view = (DualControlLayout) convertView;
            } else {
                view = (DualControlLayout) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_control_spinner_view, parent, false);
            }

            // Set up the spinner label.  The text it displays won't change.
            TextView labelView = (TextView) view.getChildAt(0);
            labelView.setText(mLabel);

            // Because the values can be of different widths, the TextView may expand or shrink.
            // Enforcing a minimum width prevents the layout from doing so as the user swaps values,
            // preventing unwanted layout passes.
            TextView valueView = (TextView) view.getChildAt(1);
            valueView.setText(getItem(position).toString());
            valueView.setMinimumWidth(mMinWidthRequiredForValues);

            return view;
        }

        /**
         * Computes and records the minimum width required to display any of the values without
         * causing another layout pass when switching values.
         */
        int computeMinWidthRequiredForValues() {
            DualControlLayout layout = getView(0, null, null);
            TextView container = (TextView) layout.getChildAt(1);

            Paint textPaint = container.getPaint();
            float longestLanguageWidth = 0;
            for (int i = 0; i < getCount(); i++) {
                float width = textPaint.measureText(getItem(i).toString());
                longestLanguageWidth = Math.max(longestLanguageWidth, width);
            }

            mMinWidthRequiredForValues = (int) Math.ceil(longestLanguageWidth);
            return mMinWidthRequiredForValues;
        }

        /**
         * Explicitly sets the minimum width required to display all of the values.
         */
        void setMinWidthRequiredForValues(int requiredWidth) {
            mMinWidthRequiredForValues = requiredWidth;
        }
    }

    /**
     * Extends the regular LayoutParams by determining where a control should be located.
     */
    @VisibleForTesting
    static final class ControlLayoutParams extends LayoutParams {
        public int start;
        public int top;
        public int columnsRequired;
        private boolean mMustBeFullWidth;

        /**
         * Stores values required for laying out this ViewGroup's children.
         *
         * This is set up as a private method to mitigate attempts at adding controls to the layout
         * that aren't provided by the InfoBarControlLayout.
         */
        private ControlLayoutParams() {
            super(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        }
    }

    private final int mMarginBetweenRows;
    private final int mMarginBetweenColumns;

    /**
     * Do not call this method directly; use {@link InfoBarLayout#addControlLayout()}.
     */
    public InfoBarControlLayout(Context context) {
        this(context, null);
    }

    public InfoBarControlLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        mMarginBetweenRows =
                resources.getDimensionPixelSize(R.dimen.infobar_control_margin_between_rows);
        mMarginBetweenColumns =
                resources.getDimensionPixelSize(R.dimen.infobar_control_margin_between_columns);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int fullWidth = MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                ? Integer.MAX_VALUE : MeasureSpec.getSize(widthMeasureSpec);
        int columnWidth = Math.max(0, (fullWidth - mMarginBetweenColumns) / 2);

        int atMostFullWidthSpec = MeasureSpec.makeMeasureSpec(fullWidth, MeasureSpec.AT_MOST);
        int exactlyFullWidthSpec = MeasureSpec.makeMeasureSpec(fullWidth, MeasureSpec.EXACTLY);
        int exactlyColumnWidthSpec = MeasureSpec.makeMeasureSpec(columnWidth, MeasureSpec.EXACTLY);
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        // Figure out how many columns each child requires.
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            measureChild(child, atMostFullWidthSpec, unspecifiedSpec);

            if (child.getMeasuredWidth() <= columnWidth
                    && !getControlLayoutParams(child).mMustBeFullWidth) {
                getControlLayoutParams(child).columnsRequired = 1;
            } else {
                getControlLayoutParams(child).columnsRequired = 2;
            }
        }

        // Pack all the children as tightly into rows as possible without changing their ordering.
        // Stretch out column-width controls if either it is the last control or the next one is
        // a full-width control.
        for (int i = 0; i < getChildCount(); i++) {
            ControlLayoutParams lp = getControlLayoutParams(getChildAt(i));

            if (i == getChildCount() - 1) {
                lp.columnsRequired = 2;
            } else {
                ControlLayoutParams nextLp = getControlLayoutParams(getChildAt(i + 1));
                if (lp.columnsRequired + nextLp.columnsRequired > 2) {
                    // This control is too big to place with the next child.
                    lp.columnsRequired = 2;
                } else {
                    // This and the next control fit on the same line.  Skip placing the next child.
                    i++;
                }
            }
        }

        // Measure all children, assuming they all have to fit within the width of the layout.
        // Height is unconstrained.
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            ControlLayoutParams lp = getControlLayoutParams(child);
            int spec = lp.columnsRequired == 1 ? exactlyColumnWidthSpec : exactlyFullWidthSpec;
            measureChild(child, spec, unspecifiedSpec);
        }

        // Pack all the children as tightly into rows as possible without changing their ordering.
        int layoutHeight = 0;
        int nextChildStart = 0;
        int nextChildTop = 0;
        int currentRowHeight = 0;
        int columnsAvailable = 2;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            ControlLayoutParams lp = getControlLayoutParams(child);

            // If there isn't enough room left for the control, move to the next row.
            if (columnsAvailable < lp.columnsRequired) {
                layoutHeight += currentRowHeight + mMarginBetweenRows;
                nextChildStart = 0;
                nextChildTop = layoutHeight;
                currentRowHeight = 0;
                columnsAvailable = 2;
            }

            lp.top = nextChildTop;
            lp.start = nextChildStart;
            currentRowHeight = Math.max(currentRowHeight, child.getMeasuredHeight());
            columnsAvailable -= lp.columnsRequired;
            nextChildStart += lp.columnsRequired * (columnWidth + mMarginBetweenColumns);
        }

        // Compute the ViewGroup's height, accounting for the final row's height.
        layoutHeight += currentRowHeight;
        setMeasuredDimension(resolveSize(fullWidth, widthMeasureSpec),
                resolveSize(layoutHeight, heightMeasureSpec));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int width = right - left;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        // Child positions were already determined during the measurement pass.
        for (int childIndex = 0; childIndex < getChildCount(); childIndex++) {
            View child = getChildAt(childIndex);
            int childLeft = getControlLayoutParams(child).start;
            if (isRtl) childLeft = width - childLeft - child.getMeasuredWidth();

            int childTop = getControlLayoutParams(child).top;
            int childRight = childLeft + child.getMeasuredWidth();
            int childBottom = childTop + child.getMeasuredHeight();
            child.layout(childLeft, childTop, childRight, childBottom);
        }
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new ControlLayoutParams();
    }

    @Override
    public LayoutParams generateLayoutParams(AttributeSet attrs) {
        return generateDefaultLayoutParams();
    }

    @Override
    protected LayoutParams generateLayoutParams(LayoutParams p) {
        return generateDefaultLayoutParams();
    }

    /**
     * Adds an icon with a descriptive message to the Title.
     *
     * -----------------------------------------------------
     * | ICON | TITLE MESSAGE                              |
     * -----------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId   ID of the drawable to use for the icon.
     * @param titleMessage     Message to display on Infobar title.
     */
    public View addIconTitle(int iconResourceId, CharSequence titleMessage) {
        LinearLayout layout =
                (LinearLayout) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_control_icon_with_description, this, false);
        addView(layout, new ControlLayoutParams());

        ImageView iconView = (ImageView) layout.findViewById(R.id.control_icon);
        iconView.setImageResource(iconResourceId);

        TextView titleView = (TextView) layout.findViewById(R.id.control_message);
        titleView.setText(titleMessage);
        titleView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                getContext().getResources().getDimension(R.dimen.infobar_text_size));

        return layout;
    }

    /**
     * Adds an icon with a descriptive message to the layout.
     *
     * -----------------------------------------------------
     * | ICON | PRIMARY MESSAGE SECONDARY MESSAGE          |
     * -----------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId   ID of the drawable to use for the icon.
     * @param iconColorId      ID of the tint color for the icon, or 0 for default.
     * @param primaryMessage   Message to display for the toggle.
     * @param secondaryMessage Additional descriptive text for the toggle.  May be null.
     */
    public View addIcon(int iconResourceId, int iconColorId, CharSequence primaryMessage,
            CharSequence secondaryMessage) {
        return addIcon(iconResourceId, iconColorId, primaryMessage, secondaryMessage,
                R.dimen.infobar_text_size);
    }

    /**
     * Adds an icon with a descriptive message to the layout.
     *
     * -----------------------------------------------------
     * | ICON | PRIMARY MESSAGE SECONDARY MESSAGE          |
     * -----------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId   ID of the drawable to use for the icon.
     * @param iconColorId      ID of the tint color for the icon, or 0 for default.
     * @param primaryMessage   Message to display for the toggle.
     * @param secondaryMessage Additional descriptive text for the toggle.  May be null.
     * @param resourceId       Size of resource id to be applied to primaryMessage
     *                         and secondaryMessage.
     */
    public View addIcon(int iconResourceId, int iconColorId, CharSequence primaryMessage,
            CharSequence secondaryMessage, int resourceId) {
        LinearLayout layout = (LinearLayout) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_icon_with_description, this, false);
        addView(layout, new ControlLayoutParams());

        ImageView iconView = (ImageView) layout.findViewById(R.id.control_icon);
        iconView.setImageResource(iconResourceId);
        if (iconColorId != 0) {
            iconView.setColorFilter(ApiCompatibilityUtils.getColor(getResources(), iconColorId));
        }

        // The primary message text is always displayed.
        TextView primaryView = (TextView) layout.findViewById(R.id.control_message);
        primaryView.setText(primaryMessage);
        primaryView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX, getContext().getResources().getDimension(resourceId));

        // The secondary message text is optional.
        TextView secondaryView =
                (TextView) layout.findViewById(R.id.control_secondary_message);
        if (secondaryMessage == null) {
            layout.removeView(secondaryView);
        } else {
            secondaryView.setText(secondaryMessage);
            secondaryView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    getContext().getResources().getDimension(resourceId));
        }

        return layout;
    }

    /**
     * Creates a standard toggle switch and adds it to the layout.
     *
     * -------------------------------------------------
     * | ICON | MESSAGE                       | TOGGLE |
     * -------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId ID of the drawable to use for the icon, or 0 to hide the ImageView.
     * @param iconColorId    ID of the tint color for the icon, or 0 for default.
     * @param toggleMessage  Message to display for the toggle.
     * @param toggleId       ID to use for the toggle.
     * @param isChecked      Whether the toggle should start off checked.
     */
    public View addSwitch(int iconResourceId, int iconColorId, CharSequence toggleMessage,
            int toggleId, boolean isChecked) {
        LinearLayout switchLayout = (LinearLayout) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_toggle, this, false);
        addView(switchLayout, new ControlLayoutParams());

        ImageView iconView = (ImageView) switchLayout.findViewById(R.id.control_icon);
        if (iconResourceId == 0) {
            switchLayout.removeView(iconView);
        } else {
            iconView.setImageResource(iconResourceId);
            if (iconColorId != 0) {
                iconView.setColorFilter(
                        ApiCompatibilityUtils.getColor(getResources(), iconColorId));
            }
        }

        TextView messageView = (TextView) switchLayout.findViewById(R.id.control_message);
        messageView.setText(toggleMessage);

        SwitchCompat switchView =
                (SwitchCompat) switchLayout.findViewById(R.id.control_toggle_switch);
        switchView.setId(toggleId);
        switchView.setChecked(isChecked);

        return switchLayout;
    }

    /**
     * Creates a set of standard radio buttons and adds it to the layout.
     *
     * @param messages      Messages to display for the options.
     * @param tags          Optional list of tags to attach to the buttons.
     */
    public RadioButtonLayout addRadioButtons(List<CharSequence> messages, @Nullable List<?> tags) {
        ControlLayoutParams params = new ControlLayoutParams();
        params.mMustBeFullWidth = true;

        RadioButtonLayout radioLayout = new RadioButtonLayout(getContext());
        radioLayout.addOptions(messages, tags);

        addView(radioLayout, params);
        return radioLayout;
    }

    /**
     * Creates a standard spinner and adds it to the layout.
     */
    public <T> Spinner addSpinner(int spinnerId, ArrayAdapter<T> arrayAdapter) {
        Spinner spinner = (Spinner) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_spinner, this, false);
        spinner.setAdapter(arrayAdapter);
        addView(spinner, new ControlLayoutParams());
        spinner.setId(spinnerId);
        return spinner;
    }

    /**
     * Creates and adds a full-width control with additional text describing what an InfoBar is for.
     */
    public View addDescription(CharSequence message) {
        ControlLayoutParams params = new ControlLayoutParams();
        params.mMustBeFullWidth = true;

        TextView descriptionView =
                (TextView) LayoutInflater.from(getContext())
                        .inflate(R.layout.dialog_control_description, this, false);
        addView(descriptionView, params);

        descriptionView.setText(message);
        descriptionView.setMovementMethod(LinkMovementMethod.getInstance());
        return descriptionView;
    }

    /**
     * Do NOT call this method directly from outside {@link InfoBarLayout#InfoBarLayout()}.
     *
     * Adds a full-width control showing the main InfoBar message.  For other text, you should call
     * {@link InfoBarControlLayout#addDescription(CharSequence)} instead.
     */
    TextView addMainMessage(CharSequence mainMessage) {
        ControlLayoutParams params = new ControlLayoutParams();
        params.mMustBeFullWidth = true;

        TextView messageView = (TextView) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_message, this, false);
        addView(messageView, params);

        messageView.setText(mainMessage);
        messageView.setMovementMethod(LinkMovementMethod.getInstance());
        return messageView;
    }

    /**
     * @return The {@link ControlLayoutParams} for the given child.
     */
    @VisibleForTesting
    static ControlLayoutParams getControlLayoutParams(View child) {
        return (ControlLayoutParams) child.getLayoutParams();
    }

}
