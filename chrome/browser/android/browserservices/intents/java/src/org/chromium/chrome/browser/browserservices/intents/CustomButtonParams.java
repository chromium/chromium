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

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.browser.customtabs.CustomTabsIntent;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Container for all parameters related to creating a customizable button. */
public interface CustomButtonParams {

    /** Enum used to describe different types of buttons. */
    @IntDef({
        ButtonType.OTHER,
        ButtonType.CCT_SHARE_BUTTON,
        ButtonType.CCT_OPEN_IN_BROWSER_BUTTON,
        ButtonType.EXTERNAL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int OTHER = 0;

        /** Share button, created by Chrome. */
        int CCT_SHARE_BUTTON = 1;

        /** Open in Browser button, created by Chrome. */
        int CCT_OPEN_IN_BROWSER_BUTTON = 2;

        /** Button from external embedding applications. */
        int EXTERNAL = 3;
    }

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
     * @return The {@link ButtonType} of the customized button.
     */
    @ButtonType
    int getType();

    /**
     * Builds an {@link ImageButton} from the data in this params. Generated buttons should be
     * placed on the bottom bar. The button's tag will be its id.
     *
     * @param parent The parent that the inflated {@link ImageButton}.
     * @param listener {@link OnClickListener} that should be used with the button.
     * @return Parsed list of {@link CustomButtonParams}, which is empty if the input is invalid.
     */
    ImageButton buildBottomBarButton(Context context, ViewGroup parent, OnClickListener listener);

    /**
     * @return Whether the given icon's size is suitable to put on toolbar.
     */
    boolean doesIconFitToolbar(Context context);

    /**
     * Updates the visibility of this component on the toolbar.
     *
     * @param showOnToolbar {@code true} to display the component on the toolbar, {@code false} to
     *     display the component on the bottomBar.
     */
    void updateShowOnToolbar(boolean showOnToolbar);
}
