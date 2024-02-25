// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;

/**
 * A radio button group used for Preload Pages. Currently, it has 3 options:
 * Extended Preloading, Standard Preloading and No Preloading.
 */
public class RadioButtonGroupPreloadPagesSettings extends Preference
        implements RadioGroup.OnCheckedChangeListener,
                RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
    /** Interface that will subscribe to Preload Pages state details requested events. */
    public interface OnPreloadPagesStateDetailsRequested {
        /**
         * Notify that details of a Preload Pages state are requested.
         * @param preloadPagesState The Preload Pages state that is requested for more details.
         */
        void onPreloadPagesStateDetailsRequested(@PreloadPagesState int preloadPagesState);
    }

    private RadioButtonWithDescriptionAndAuxButton mExtendedPreloading;
    private RadioButtonWithDescriptionAndAuxButton mStandardPreloading;
    private RadioButtonWithDescription mNoPreloading;
    private @PreloadPagesState int mPreloadPagesState;
    private OnPreloadPagesStateDetailsRequested mPreloadPagesStateDetailsRequestedListener;
    private ManagedPreferenceDelegate mManagedPrefDelegate;

    public RadioButtonGroupPreloadPagesSettings(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.radio_button_group_preload_pages_preference);
    }

    /**
     * Set Preload Pages state. Called before onBindViewHolder.
     * @param preloadPagesState The current Preload Pages state.
     */
    public void init(@PreloadPagesState int preloadPagesState) {
        mPreloadPagesState = preloadPagesState;
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        if (checkedId == mExtendedPreloading.getId()) {
            mPreloadPagesState = PreloadPagesState.EXTENDED_PRELOADING;
        } else if (checkedId == mStandardPreloading.getId()) {
            mPreloadPagesState = PreloadPagesState.STANDARD_PRELOADING;
        } else if (checkedId == mNoPreloading.getId()) {
            mPreloadPagesState = PreloadPagesState.NO_PRELOADING;
        } else {
            assert false : "Should not be reached.";
        }
        callChangeListener(mPreloadPagesState);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mExtendedPreloading =
                (RadioButtonWithDescriptionAndAuxButton)
                        holder.findViewById(R.id.extended_preloading);
        mExtendedPreloading.setAuxButtonClickedListener(this);
        mStandardPreloading =
                (RadioButtonWithDescriptionAndAuxButton)
                        holder.findViewById(R.id.standard_preloading);
        mStandardPreloading.setAuxButtonClickedListener(this);
        mNoPreloading = (RadioButtonWithDescription) holder.findViewById(R.id.no_preloading);
        RadioButtonWithDescriptionLayout groupLayout =
                (RadioButtonWithDescriptionLayout) mNoPreloading.getRootView();
        groupLayout.setOnCheckedChangeListener(this);

        setCheckedState(mPreloadPagesState);

        // If Preload Pages is managed, disable the radio button group, but keep the aux buttons
        // enabled to disclose information.
        if (mManagedPrefDelegate.isPreferenceClickDisabled(this)) {
            groupLayout.setEnabled(false);
            mExtendedPreloading.setAuxButtonEnabled(true);
            mStandardPreloading.setAuxButtonEnabled(true);
        }
    }

    @Override
    public void onAuxButtonClicked(int clickedButtonId) {
        if (clickedButtonId == mExtendedPreloading.getId()) {
            mPreloadPagesStateDetailsRequestedListener.onPreloadPagesStateDetailsRequested(
                    PreloadPagesState.EXTENDED_PRELOADING);
        } else if (clickedButtonId == mStandardPreloading.getId()) {
            mPreloadPagesStateDetailsRequestedListener.onPreloadPagesStateDetailsRequested(
                    PreloadPagesState.STANDARD_PRELOADING);
        } else {
            assert false : "Should not be reached.";
        }
    }

    /**
     * Sets a listener that will be notified when details of a Preload Pages state are requested.
     * @param listener New listener that will be notified when details of a Preload Pages state are
     *         requested.
     */
    public void setPreloadPagesStateDetailsRequestedListener(
            OnPreloadPagesStateDetailsRequested listener) {
        mPreloadPagesStateDetailsRequestedListener = listener;
    }

    /**
     * Sets the ManagedPreferenceDelegate which will determine whether this preference is managed.
     */
    public void setManagedPreferenceDelegate(ManagedPreferenceDelegate delegate) {
        mManagedPrefDelegate = delegate;
        // The value of `allowManagedIcon` doesn't matter, because the corresponding layout doesn't
        // define an icon view.
        ManagedPreferencesUtils.initPreference(
                mManagedPrefDelegate,
                this,
                /* allowManagedIcon= */ true,
                /* hasCustomLayout= */ true);
    }

    /**
     * Sets the checked state of the Preload Pages radio button group.
     * @param checkedState Set the radio button of checkedState to checked, and set the radio
     *         buttons of other states to unchecked.
     */
    public void setCheckedState(@PreloadPagesState int checkedState) {
        mPreloadPagesState = checkedState;
        mExtendedPreloading.setChecked(checkedState == PreloadPagesState.EXTENDED_PRELOADING);
        mStandardPreloading.setChecked(checkedState == PreloadPagesState.STANDARD_PRELOADING);
        mNoPreloading.setChecked(checkedState == PreloadPagesState.NO_PRELOADING);
    }

    public @PreloadPagesState int getPreloadPagesStateForTesting() {
        return mPreloadPagesState;
    }

    public RadioButtonWithDescriptionAndAuxButton getExtendedPreloadingButtonForTesting() {
        return mExtendedPreloading;
    }

    public RadioButtonWithDescriptionAndAuxButton getStandardPreloadingButtonForTesting() {
        return mStandardPreloading;
    }

    public RadioButtonWithDescription getNoPreloadingButtonForTesting() {
        return mNoPreloading;
    }
}
