// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.NonNull;
import androidx.browser.customtabs.CustomTabsIntent;

/** Container for all parameters related to creating a customizable button. */
public interface CustomButtonParams {
    /** Replaces the current icon and description with new ones. */
    void update(@NonNull Bitmap icon, @NonNull String description);

    /**
     * @return Whether this button should be shown on the toolbar.
     */
    boolean showOnToolbar();

    /**
     * @return The id associated with this button. The custom button on the toolbar always uses
     *         {@link CustomTabsIntent#TOOLBAR_ACTION_BUTTON_ID} as id.
     */
    int getId();

    /**
     * @return The drawable for the customized button.
     */
    Drawable getIcon(Context context);

    /**
     * @return The content description for the customized button.
     */
    String getDescription();

    /**
     * @return The {@link PendingIntent} that will be sent when user clicks the customized button.
     */
    PendingIntent getPendingIntent();

    /**
     * Builds an {@link ImageButton} from the data in this params. Generated buttons should be
     * placed on the bottom bar. The button's tag will be its id.
     * @param parent The parent that the inflated {@link ImageButton}.
     * @param listener {@link OnClickListener} that should be used with the button.
     * @return Parsed list of {@link CustomButtonParams}, which is empty if the input is invalid.
     */
    ImageButton buildBottomBarButton(Context context, ViewGroup parent, OnClickListener listener);

    /**
     * @return Whether the given icon's size is suitable to put on toolbar.
     */
    boolean doesIconFitToolbar(Context context);
}
