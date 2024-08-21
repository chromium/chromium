// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;

/** Data for the {@link TabCardLabelView}. */
public class TabCardLabelData {
    public final @TabCardLabelType int labelType;
    public final TextResolver textResolver;
    public final @Nullable AsyncImageView.Factory asyncImageFactory;
    public final @Nullable TextResolver contentDescriptionResolver;

    /**
     * @param labelType The {@link TabCardLabelType} for the appearance of the card.
     * @param textResolver A {@link TextResolver} to resolve the text.
     * @param asyncImageFactory To produce icon images. If null no icon is provided.
     * @param contentDescriptionResolver A {@link TextResolver} for the content description. Passing
     *     null will use the default content description of the text in the {@code textResolver}.
     */
    public TabCardLabelData(
            @TabCardLabelType int labelType,
            TextResolver textResolver,
            @Nullable AsyncImageView.Factory asyncImageFactory,
            @Nullable TextResolver contentDescriptionResolver) {
        this.labelType = labelType;
        this.textResolver = textResolver;
        this.asyncImageFactory = asyncImageFactory;
        this.contentDescriptionResolver = contentDescriptionResolver;
    }
}
