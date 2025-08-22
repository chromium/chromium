// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.graphics.Paint.FontMetricsInt;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.style.LineHeightSpan;
import android.text.style.TextAppearanceSpan;
import android.text.style.TypefaceSpan;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.slider.Slider;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.text.NumberFormat;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * A view which displays preferences for reader mode. This allows users to change the theme, font
 * size, etc. when browsing in reader mode. This has the same functionality as
 * DistilledPagePrefsView, but created to support new UI.
 */
@NullMarked
public class ReaderModePrefsView extends LinearLayout
        implements DistilledPagePrefs.Observer, View.OnClickListener {
    // XML layout for View.
    private static final int VIEW_LAYOUT = R.layout.reader_mode_prefs_view;

    // Constants for font scaling slider.
    private static final float FONT_SCALE_LOWER_BOUND = 1.0f;
    private static final float FONT_SCALE_UPPER_BOUND = 2.5f;
    private static final float FONT_SCALE_STEP_SIZE = 0.25f;

    // Constants for setting the font style button typefaces.
    private static final String SANS_SERIF_TYPEFACE_NAME = "sans-serif";
    private static final String SERIF_TYPEFACE_NAME = "serif";
    private static final String MONOSPACE_TYPEFACE_NAME = "monospace";

    // Buttons for theme.
    private final Map<Integer/* Theme= */ , MaterialButton> mThemeButtons;
    // Border widths of theme buttons.
    private final int mDefaultStrokeWidth;
    private final int mSelectedStrokeWidth;

    // Buttons for font family.
    private final Map<Integer/* FontFamily= */ , MaterialButton> mFontFamilyButtons;

    private final NumberFormat mPercentageFormatter;

    private Slider mFontScalingSlider;

    private DistilledPagePrefs mDistilledPagePrefs;

    /**
     * Creates a ReaderModePrefsView.
     *
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public ReaderModePrefsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mThemeButtons = new HashMap<Integer/* Theme= */ , MaterialButton>();
        mFontFamilyButtons = new HashMap<Integer/* FontFamily= */ , MaterialButton>();
        mPercentageFormatter = NumberFormat.getPercentInstance(Locale.getDefault());
        mDefaultStrokeWidth =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.reader_mode_theme_button_unselected_stroke_width);
        mSelectedStrokeWidth =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.reader_mode_theme_button_selected_stroke_width);
    }

    /**
     * Creates a ReaderModePrefsView. This is the method for programmatically creating this view.
     *
     * @param context The {@link Context} to use for inflation.
     * @return A new {@link ReaderModePrefsView}.
     */
    public static ReaderModePrefsView create(
            Context context, DistilledPagePrefs distilledPagePrefs) {
        ReaderModePrefsView prefsView =
                (ReaderModePrefsView) LayoutInflater.from(context).inflate(VIEW_LAYOUT, null);
        prefsView.initDistilledPagePrefs(distilledPagePrefs);
        return prefsView;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        initializeFontButton(R.id.font_sans_serif, FontFamily.SANS_SERIF);
        initializeFontButton(R.id.font_serif, FontFamily.SERIF);
        initializeFontButton(R.id.font_monospace, FontFamily.MONOSPACE);

        mFontScalingSlider = findViewById(R.id.font_size);
        mFontScalingSlider.setValueFrom(FONT_SCALE_LOWER_BOUND);
        mFontScalingSlider.setValueTo(FONT_SCALE_UPPER_BOUND);
        mFontScalingSlider.setStepSize(FONT_SCALE_STEP_SIZE);
        mFontScalingSlider.setTickActiveRadius(0);
        mFontScalingSlider.setTickInactiveRadius(0);
        mFontScalingSlider.setLabelFormatter(value -> mPercentageFormatter.format(value));

        mFontScalingSlider.addOnChangeListener(
                (slider, value, fromUser) -> {
                    if (fromUser) {
                        ReaderModeMetrics.reportReaderModePrefsFontScalingChanged(value);
                        mDistilledPagePrefs.setFontScaling(value);
                    }
                });

        initializeColorButton(R.id.light_mode, Theme.LIGHT);
        initializeColorButton(R.id.dark_mode, Theme.DARK);
        initializeColorButton(R.id.sepia_mode, Theme.SEPIA);
    }

    /**
     * Creates SpannableString for the font style button with two lines of text.
     *
     * @param line1 The text for the first line (e.g., "Aa").
     * @param line2 The text for the second line (e.g., "Sans Serif").
     * @return A SpannableString with the specified styling.
     */
    private SpannableStringBuilder createStyledButtonText(
            String line1, String line2, String typeface) {
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
        SpannableString fontStyleSignifierString = new SpannableString(line1);
        SpannableString fontStyleNameString = new SpannableString(line2);

        fontStyleSignifierString.setSpan(
                new TypefaceSpan(typeface), 0, line1.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        fontStyleNameString.setSpan(
                new TypefaceSpan(typeface), 0, line2.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        fontStyleSignifierString.setSpan(
                new TextAppearanceSpan(
                        getContext(), R.style.TextAppearance_ReaderModePrefsFontStyleSignifier),
                0,
                line1.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        fontStyleNameString.setSpan(
                new TextAppearanceSpan(
                        getContext(), R.style.TextAppearance_ReaderModePrefsFontStyleName),
                0,
                line2.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Get font metrics for first line using mock TextPaint to set suitable line height.
        TextPaint mockTextPaint = new TextPaint();
        mockTextPaint.setTextSize(
                getContext()
                        .getResources()
                        .getDimension(R.dimen.reader_mode_prefs_font_style_signifier_text_size));
        FontMetricsInt fontMetrics = mockTextPaint.getFontMetricsInt();
        final int fontHeightPx = fontMetrics.descent - fontMetrics.ascent;
        final int fontDescentPx = fontMetrics.descent;
        fontStyleSignifierString.setSpan(
                new LineHeightSpan() {
                    @Override
                    public void chooseHeight(
                            CharSequence text,
                            int start,
                            int end,
                            int spanstartv,
                            int v,
                            FontMetricsInt fm) {
                        fm.ascent = -(fontHeightPx - fontDescentPx);
                        fm.descent = fontDescentPx;
                        fm.top = fm.ascent;
                        fm.bottom = fm.descent;
                    }
                },
                0,
                line1.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        spannableStringBuilder.append(fontStyleSignifierString);
        spannableStringBuilder.append("\n");
        spannableStringBuilder.append(fontStyleNameString);
        return spannableStringBuilder;
    }

    @Initializer
    private void initDistilledPagePrefs(DistilledPagePrefs distilledPagePrefs) {
        assert distilledPagePrefs != null;
        mDistilledPagePrefs = distilledPagePrefs;

        onChangeFontFamily(mDistilledPagePrefs.getFontFamily());
        onChangeFontScaling(mDistilledPagePrefs.getFontScaling());
        onChangeTheme(mDistilledPagePrefs.getTheme());
    }

    private void initializeColorButton(@IdRes int id, final int theme) {
        Theme.validate(theme);
        MaterialButton button = findViewById(id);
        button.setOnClickListener(
                v -> {
                    ReaderModeMetrics.reportReaderModePrefsThemeChanged(theme);
                    mDistilledPagePrefs.setUserPrefTheme(theme);
                });
        mThemeButtons.put(theme, button);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mDistilledPagePrefs.addObserver(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mDistilledPagePrefs.removeObserver(this);
    }

    // DistilledPagePrefs.Observer

    @Override
    public void onChangeFontFamily(int fontFamily) {
        FontFamily.validate(fontFamily);
        for (Map.Entry<Integer, MaterialButton> entry : mFontFamilyButtons.entrySet()) {
            boolean isSelected = entry.getKey() == fontFamily;
            entry.getValue().setChecked(isSelected);
        }
    }

    @Override
    public void onChangeTheme(int theme) {
        Theme.validate(theme);
        for (Map.Entry<Integer, MaterialButton> entry : mThemeButtons.entrySet()) {
            boolean isSelected = entry.getKey() == theme;
            entry.getValue().setChecked(isSelected);
            // We do not want borders on light and dark mode theme buttons in unselected state.
            int themeButtonUnselectedBorderWidth =
                    entry.getValue().getId() == R.id.sepia_mode ? mDefaultStrokeWidth : 0;
            entry.getValue()
                    .setStrokeWidth(
                            isSelected ? mSelectedStrokeWidth : themeButtonUnselectedBorderWidth);
        }
    }

    @Override
    public void onChangeFontScaling(float scaling) {
        // Due to changing font scale step size from 0.05 to 0.25, slider will crash if provided a
        // non-multiple of step size.
        // If user has previous preference saved at a non-step size interval, need to adjust to
        // closest increment.
        scaling =
                Math.max(
                        (Math.round(scaling / FONT_SCALE_STEP_SIZE) * FONT_SCALE_STEP_SIZE),
                        FONT_SCALE_LOWER_BOUND);

        mFontScalingSlider.setValue(scaling);

        // Update the content description for accessibility.
        String userFriendlyFontSizeDescription =
                getContext()
                        .getString(
                                R.string.font_size_accessibility_label,
                                mPercentageFormatter.format(scaling));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            mFontScalingSlider.setStateDescription(userFriendlyFontSizeDescription);
        } else {
            mFontScalingSlider.setContentDescription(userFriendlyFontSizeDescription);
        }
    }

    private void initializeFontButton(@IdRes int id, final int fontFamily) {
        FontFamily.validate(fontFamily);
        MaterialButton button = findViewById(id);
        String line1 = getContext().getString(R.string.font_style_signifier);
        String line2 = "";
        String typeface = "";
        switch (fontFamily) {
            case FontFamily.SANS_SERIF:
                line2 = getContext().getString(R.string.sans_serif_lowercase);
                typeface = SANS_SERIF_TYPEFACE_NAME;
                break;
            case FontFamily.SERIF:
                line2 = getContext().getString(R.string.serif);
                typeface = SERIF_TYPEFACE_NAME;
                break;
            case FontFamily.MONOSPACE:
                line2 = getContext().getString(R.string.monospace_shortened);
                typeface = MONOSPACE_TYPEFACE_NAME;
                break;
            default:
                assert false : "Invalid font family provided: " + fontFamily;
        }
        button.setText(createStyledButtonText(line1, line2, typeface));
        button.setMaxLines(2);
        button.setOnClickListener(this);
        button.setTag(fontFamily);
        mFontFamilyButtons.put(fontFamily, button);
    }

    @Override
    public void onClick(View view) {
        int fontFamily = (int) view.getTag();
        FontFamily.validate(fontFamily);
        ReaderModeMetrics.reportReaderModePrefsFontFamilyChanged(fontFamily);
        mDistilledPagePrefs.setFontFamily(fontFamily);
    }
}
