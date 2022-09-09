// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/**
 * A common utils class for Message cards for updating the look of different UI elements present
 * inside the message card view.
 */
public class MessageCardViewUtils {
    /**
     * Set text appearance for title.
     *
     * @param title The title whose text appearance we want to modify.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isLargeMessageCard Whether the message card view requesting is a large message card.
     */
    public static void setTitleTextAppearance(
            TextView title, boolean isIncognito, boolean isLargeMessageCard) {
        int titleTextAppearance = (isLargeMessageCard)
                ? TabUiThemeProvider.getLargeMessageCardTitleTextAppearance(isIncognito)
                : TabUiThemeProvider.getMessageCardTitleTextAppearance(isIncognito);
        ApiCompatibilityUtils.setTextAppearance(title, titleTextAppearance);
    }

    /**
     * Set text appearance for description.
     *
     * @param description The description whose text appearance we want to modify.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isLargeMessageCard Whether the message card view requesting is a large message card.
     */
    public static void setDescriptionTextAppearance(
            TextView description, boolean isIncognito, boolean isLargeMessageCard) {
        int descriptionTextAppearance = (isLargeMessageCard)
                ? TabUiThemeProvider.getLargeMessageCardDescriptionTextAppearance(isIncognito)
                : TabUiThemeProvider.getMessageCardDescriptionTextAppearance(isIncognito);
        ApiCompatibilityUtils.setTextAppearance(description, descriptionTextAppearance);
    }

    /**
     * Set appearance for action button.
     *
     * @param actionButton The button whose text appearance we want to modify.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     * @param isLargeMessageCard Whether the message card view requesting is a large message card.
     */
    public static void setActionButtonTextAppearance(
            ButtonCompat actionButton, boolean isIncognito, boolean isLargeMessageCard) {
        int actionButtonTextAppearance = (isLargeMessageCard)
                ? TabUiThemeProvider.getLargeMessageCardActionButtonTextAppearance(isIncognito)
                : TabUiThemeProvider.getMessageCardActionButtonTextAppearance(isIncognito);

        ApiCompatibilityUtils.setTextAppearance(actionButton, actionButtonTextAppearance);
    }

    /**
     * Set background color for action button.
     *
     * @param actionButton The button whose background color we want to modify.
     * @param isIncognito Whether the background color is used for incognito mode.
     */
    public static void setActionButtonBackgroundColor(
            ButtonCompat actionButton, boolean isIncognito, boolean isLargeMessageCard) {
        if (!isLargeMessageCard) {
            assert false : "Currently not supported.";
            return;
        }
        actionButton.setButtonColor(
                ColorStateList.valueOf(TabUiThemeProvider.getLargeMessageCardActionButtonColor(
                        actionButton.getContext(), isIncognito)));
    }

    /**
     * Set text appearance for secondary action button.
     *
     * @param secondaryActionButton The button whose text appearance we want to modify.
     * @param isIncognito Whether the text appearance is used for incognito mode.
     */
    public static void setSecondaryActionButtonColor(
            ButtonCompat secondaryActionButton, boolean isIncognito) {
        secondaryActionButton.setTextColor(
                TabUiThemeProvider.getMessageCardSecondaryActionButtonColor(
                        secondaryActionButton.getContext(), isIncognito));
    }

    /**
     * Set tint for close button.
     *
     * TODO(crbug.com/1139194): Set action button ripple color.
     *
     * @param closeButton The close button image view whose tint we want to set.
     * @param isIncognito Whether the tint is used for incognito mode.
     */
    public static void setCloseButtonTint(ChromeImageView closeButton, boolean isIncognito) {
        ApiCompatibilityUtils.setImageTintList(closeButton,
                TabUiThemeProvider.getMessageCardCloseButtonTintList(
                        closeButton.getContext(), isIncognito));
    }
}
