// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A view that shows a permission prompt for auto picture-in-picture in the PiP window.
 *
 * <p>This view is designed to be shown as an overlay on top of the Picture-in-Picture window
 * content, along with {@link AutoPictureInPicturePrivacyMaskView} which is required to obscure
 * potentially sensitive content behind the prompt.
 *
 * <p>Since this prompt is tied to the creation of the PiP window and is the only UI shown upon
 * trigger, there is no contention with other dialogs that would require the queueing capabilities
 * of {@link org.chromium.ui.modaldialog.ModalDialogManager}.
 */
@NullMarked
public class AutoPipPermissionDialogView extends LinearLayout {
    /** The result of the user's interaction with the permission prompt. */
    @IntDef({UiResult.ALLOW_ON_EVERY_VISIT, UiResult.ALLOW_ONCE, UiResult.BLOCK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UiResult {
        int ALLOW_ON_EVERY_VISIT = 0;
        int ALLOW_ONCE = 1;
        int BLOCK = 2;
    }

    private final ImageView mIconView;
    private final TextView mTitle;
    private final LinearLayout mButtonContainer;

    /**
     * Constructs a new AutoPipPermissionDialogView.
     *
     * @param context The context used to inflate the view.
     * @param allowEveryVisitText The text for the button that allows the action on every visit.
     * @param allowOnceText The text for the button that allows the action for the current session.
     * @param blockText The text for the button that blocks the action.
     * @param resultCb The callback to be invoked when the user makes a choice.
     */
    public AutoPipPermissionDialogView(
            Context context,
            String allowEveryVisitText,
            String allowOnceText,
            String blockText,
            Callback<Integer> resultCb) {
        super(context);

        // Center the dialog view in its parent.
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER);
        // Minimum android popup window width is 385dp.
        params.width =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.auto_pip_permission_dialog_width);
        setLayoutParams(params);

        setOrientation(LinearLayout.VERTICAL);
        setGravity(Gravity.CENTER_HORIZONTAL);
        setBackgroundResource(R.drawable.auto_pip_permission_dialog_background);
        int padding =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.auto_pip_permission_dialog_padding);
        setPadding(padding, padding, padding, padding);

        ContextThemeWrapper contextThemeWrapper =
                new ContextThemeWrapper(context, R.style.Theme_Chromium_DialogWhenLarge);
        LayoutInflater.from(contextThemeWrapper)
                .inflate(R.layout.auto_pip_permission_dialog, this, true);

        mIconView = findViewById(R.id.auto_pip_permission_dialog_icon);
        mIconView.setImageResource(R.drawable.picture_in_picture_24px);
        mTitle = findViewById(R.id.auto_pip_permission_dialog_title);
        mButtonContainer = findViewById(R.id.auto_pip_button_container);

        // Create and add buttons programmatically to apply the correct styles.
        ButtonCompat allowEveryVisitButton =
                new ButtonCompat(context, R.style.FilledButton_Tonal_ThemeOverlay_TopButton);
        allowEveryVisitButton.setText(allowEveryVisitText);
        allowEveryVisitButton.setOnClickListener(
                (v) -> resultCb.onResult(UiResult.ALLOW_ON_EVERY_VISIT));
        mButtonContainer.addView(allowEveryVisitButton);

        ButtonCompat allowOnceButton =
                new ButtonCompat(context, R.style.FilledButton_Tonal_ThemeOverlay_MiddleButton);
        allowOnceButton.setText(allowOnceText);
        allowOnceButton.setOnClickListener((v) -> resultCb.onResult(UiResult.ALLOW_ONCE));
        mButtonContainer.addView(allowOnceButton);

        ButtonCompat blockButton =
                new ButtonCompat(context, R.style.FilledButton_Tonal_ThemeOverlay_BottomButton);
        blockButton.setText(blockText);
        blockButton.setOnClickListener((v) -> resultCb.onResult(UiResult.BLOCK));
        mButtonContainer.addView(blockButton);
    }

    /**
     * Sets the origin to be displayed in the permission prompt.
     *
     * @param origin The origin string to display.
     */
    public void setOrigin(String origin) {
        mTitle.setText(
                getContext().getString(R.string.auto_picture_in_picture_prompt_title, origin));
    }
}
