// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A chip to display to the user.
 */
public class AssistantChip {
    @IntDef({Type.CHIP_ASSISTIVE, Type.BUTTON_FILLED_BLUE, Type.BUTTON_HAIRLINE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int CHIP_ASSISTIVE = 0;
        int BUTTON_FILLED_BLUE = 1;
        int BUTTON_HAIRLINE = 2;

        /** The number of types. Increment this value if you add a type. */
        int NUM_ENTRIES = 3;
    }

    /**
     * An icon that should be displayed next to the text. This is the java version of the ChipIcon
     * enum in //components/autofill_assistant/browser/service.proto. DO NOT change this without
     * adapting that proto enum.
     */
    @IntDef({Icon.NONE, Icon.CLEAR, Icon.DONE, Icon.REFRESH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Icon {
        int NONE = 0;

        // https://icons.googleplex.com/#icon=ic_clear
        int CLEAR = 1;

        // https://icons.googleplex.com/#icon=ic_done
        int DONE = 2;

        // https://icons.googleplex.com/#icon=ic_refresh
        int REFRESH = 3;
    }

    /**
     * The type of the chip. This will impact the background, border and text colors of the chip.
     */
    private final @Type int mType;

    /** The icon, shown next to the text.*/
    private final @Icon int mIcon;

    /** The text displayed on the chip. */
    private final String mText;

    /** Whether this chip is enabled or not. */
    private boolean mDisabled;

    /** Whether this chip is visible or not. */
    private boolean mVisible;

    /**
     * Whether this chip is sticky. A sticky chip will be a candidate to be displayed in the header
     * if the peek mode of the sheet is HANDLE_HEADER.
     */
    private final boolean mSticky;

    /** The callback that will be triggered when this chip is clicked. */
    private final Runnable mSelectedListener;

    public AssistantChip(@Type int type, @Icon int icon, String text, boolean disabled,
            boolean sticky, boolean visible, Runnable selectedListener) {
        mType = type;
        mIcon = icon;
        mText = text;
        mDisabled = disabled;
        mSticky = sticky;
        mVisible = visible;
        mSelectedListener = selectedListener;
    }

    public int getType() {
        return mType;
    }

    public int getIcon() {
        return mIcon;
    }

    public String getText() {
        return mText;
    }

    public boolean isDisabled() {
        return mDisabled;
    }

    public boolean isVisible() {
        return mVisible;
    }

    public void setDisabled(boolean disabled) {
        mDisabled = disabled;
    }

    public void setVisible(boolean visible) {
        mVisible = visible;
    }

    public boolean isSticky() {
        return mSticky;
    }

    public Runnable getSelectedListener() {
        return mSelectedListener;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof AssistantChip)) {
            return false;
        }

        AssistantChip that = (AssistantChip) other;
        return this.getType() == that.getType() && this.getText().equals(that.getText())
                && this.getIcon() == that.getIcon() && this.isSticky() == that.isSticky()
                && this.isDisabled() == that.isDisabled() && this.isVisible() == that.isVisible();
    }
}
