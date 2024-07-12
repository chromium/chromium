// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.graphics.Typeface;
import android.os.Build;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.text.NumberFormat;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * A view which displays preferences for distilled pages.  This allows users
 * to change the theme, font size, etc. of distilled pages.
 */
public class DistilledPagePrefsView extends LinearLayout
        implements DistilledPagePrefs.Observer, SeekBar.OnSeekBarChangeListener {
    // XML layout for View.
    private static final int VIEW_LAYOUT = R.layout.distilled_page_prefs_view;

    // Buttons for color mode.
    private final Map<Integer /* Theme */, RadioButton> mColorModeButtons;

    private final NumberFormat mPercentageFormatter;

    // RadioGroup for color mode buttons.
    private RadioGroup mRadioGroup;

    private DistilledPagePrefs mDistilledPagePrefs;

    // Text field showing font scale percentage.
    private TextView mFontScaleTextView;

    // SeekBar for font scale. Has range of [0, 30].
    private SeekBar mFontScaleSeekBar;

    // Spinner for choosing a font family.
    private Spinner mFontFamilySpinner;

    /**
     * Creates a DistilledPagePrefsView.
     *
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public DistilledPagePrefsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mColorModeButtons = new HashMap<Integer /* Theme */, RadioButton>();
        mPercentageFormatter = NumberFormat.getPercentInstance(Locale.getDefault());
    }

    public static DistilledPagePrefsView create(
            Context context, DistilledPagePrefs distilledPagePrefs) {
        DistilledPagePrefsView prefsView =
                (DistilledPagePrefsView) LayoutInflater.from(context).inflate(VIEW_LAYOUT, null);
        prefsView.initDistilledPagePrefs(distilledPagePrefs);
        return prefsView;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mRadioGroup = findViewById(R.id.radio_button_group);
        mColorModeButtons.put(Theme.LIGHT, initializeAndGetButton(R.id.light_mode, Theme.LIGHT));
        mColorModeButtons.put(Theme.DARK, initializeAndGetButton(R.id.dark_mode, Theme.DARK));
        mColorModeButtons.put(Theme.SEPIA, initializeAndGetButton(R.id.sepia_mode, Theme.SEPIA));

        mFontScaleSeekBar = findViewById(R.id.font_size);
        mFontScaleTextView = findViewById(R.id.font_size_percentage);

        mFontFamilySpinner = findViewById(R.id.font_family);
    }

    private void initDistilledPagePrefs(DistilledPagePrefs distilledPagePrefs) {
        assert distilledPagePrefs != null;
        mDistilledPagePrefs = distilledPagePrefs;

        mColorModeButtons.get(mDistilledPagePrefs.getTheme()).setChecked(true);
        initFontFamilySpinner();

        // Setting initial progress on font scale seekbar.
        onChangeFontScaling(mDistilledPagePrefs.getFontScaling());
        mFontScaleSeekBar.setOnSeekBarChangeListener(this);
    }

    private void initFontFamilySpinner() {
        // These must be kept in sync (and in-order) with
        // components/dom_distiller/core/font_family_list.h
        // TODO(wychen): fix getStringArray issue (https://crbug/803117#c2)
        String[] fonts = {
            getResources().getString(R.string.sans_serif),
            getResources().getString(R.string.serif),
            getResources().getString(R.string.monospace)
        };
        ArrayAdapter<CharSequence> adapter =
                new ArrayAdapter<CharSequence>(
                        getContext(), android.R.layout.simple_spinner_item, fonts) {
                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        View view = super.getView(position, convertView, parent);
                        return overrideTypeFace(view, position);
                    }

                    @Override
                    public View getDropDownView(int position, View convertView, ViewGroup parent) {
                        View view = super.getDropDownView(position, convertView, parent);
                        return overrideTypeFace(view, position);
                    }

                    private View overrideTypeFace(View view, int family) {
                        FontFamily.validate(family);
                        if (view instanceof TextView) {
                            TextView textView = (TextView) view;
                            if (family == FontFamily.MONOSPACE) {
                                textView.setTypeface(Typeface.MONOSPACE);
                            } else if (family == FontFamily.SANS_SERIF) {
                                textView.setTypeface(Typeface.SANS_SERIF);
                            } else if (family == FontFamily.SERIF) {
                                textView.setTypeface(Typeface.SERIF);
                            }
                        }
                        return view;
                    }
                };

        adapter.setDropDownViewResource(R.layout.distilled_page_font_family_spinner);
        mFontFamilySpinner.setAdapter(adapter);
        mFontFamilySpinner.setSelection(mDistilledPagePrefs.getFontFamily());
        mFontFamilySpinner.setOnItemSelectedListener(
                new OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(
                            AdapterView<?> parent, View view, int family, long id) {
                        if (FontFamily.isKnownValue(family)) {
                            mDistilledPagePrefs.setFontFamily(family);
                        }
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> parent) {
                        // Nothing to do.
                    }
                });
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mRadioGroup.setOrientation(HORIZONTAL);

        for (RadioButton button : mColorModeButtons.values()) {
            ViewGroup.LayoutParams layoutParams = button.getLayoutParams();
            layoutParams.width = 0;
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // If text is wider than button, change layout so that buttons are stacked on
        // top of each other.
        for (RadioButton button : mColorModeButtons.values()) {
            if (button.getLineCount() > 1) {
                mRadioGroup.setOrientation(VERTICAL);
                for (RadioButton innerLoopButton : mColorModeButtons.values()) {
                    ViewGroup.LayoutParams layoutParams = innerLoopButton.getLayoutParams();
                    layoutParams.width = LayoutParams.MATCH_PARENT;
                }
                break;
            }
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
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
        mFontFamilySpinner.setSelection(fontFamily);
    }

    /** Changes which button is selected if the theme is changed in another tab. */
    @Override
    public void onChangeTheme(int theme) {
        Theme.validate(theme);
        mColorModeButtons.get(theme).setChecked(true);
    }

    @Override
    public void onChangeFontScaling(float scaling) {
        setFontScaleTextView(scaling);
        setFontScaleProgress(scaling);
    }

    // SeekBar.OnSeekBarChangeListener

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        // progress = [0, 30]
        // newValue = .50, .55, .60, ..., 1.95, 2.00 (supported font scales)
        float newValue = (progress / 20f + .5f);
        setFontScaleTextView(newValue);
        if (fromUser) {
            mDistilledPagePrefs.setFontScaling(newValue);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {}

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {}

    /** Initiatializes a Button and selects it if it corresponds to the current theme. */
    private RadioButton initializeAndGetButton(int id, final int theme) {
        Theme.validate(theme);
        final RadioButton button = findViewById(id);
        button.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        mDistilledPagePrefs.setTheme(theme);
                    }
                });
        return button;
    }

    /** Sets the progress of mFontScaleSeekBar. */
    private void setFontScaleProgress(float newValue) {
        // newValue = .50, .55, .60, ..., 1.95, 2.00 (supported font scales)
        // progress = [0, 30]
        int progress = (int) Math.round((newValue - .5) * 20);
        mFontScaleSeekBar.setProgress(progress);

        // On Android R+, use stateDescription so the only percentage announced to the user is
        // the scaling percent. For previous versions the SeekBar percentage is always announced.
        String userFriendlyFontDescription =
                getContext()
                        .getResources()
                        .getString(
                                R.string.font_size_accessibility_label,
                                mPercentageFormatter.format(newValue));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            mFontScaleSeekBar.setStateDescription(userFriendlyFontDescription);
        } else {
            mFontScaleSeekBar.setContentDescription(userFriendlyFontDescription);
        }
    }

    /** Sets the text for the font scale text view. */
    private void setFontScaleTextView(float newValue) {
        mFontScaleTextView.setText(mPercentageFormatter.format(newValue));
    }
}
