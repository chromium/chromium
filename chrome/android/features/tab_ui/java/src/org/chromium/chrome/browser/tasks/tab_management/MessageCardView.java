// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.snackbar.TemplatePreservingTextView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.lang.ref.WeakReference;

/**
 * Represents a secondary card view in Grid Tab Switcher. The view contains an icon, a description,
 * an action button for acceptance, and a close button for dismissal.
 */
class MessageCardView extends LinearLayout {
    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    /**
     * An interface to get the icon to be shown inside the message card.
     */
    public interface IconProvider { Drawable getIconDrawable(); }

    /**
     * An interface to handle the review action.
     */
    public interface ReviewActionProvider { void review(); }

    /**
     * An interface to handle the dismiss action.
     */
    public interface DismissActionProvider { void dismiss(); }

    private ChromeImageView mIcon;
    private TemplatePreservingTextView mDescription;
    private ButtonCompat mActionButton;
    private ChromeImageView mCloseButton;

    public MessageCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIcon = findViewById(R.id.icon);
        mDescription = findViewById(R.id.description);
        mActionButton = findViewById(R.id.action_button);
        mCloseButton = findViewById(R.id.close_button);

        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
        }
        mCloseButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    /**
     * @see TemplatePreservingTextView#setTemplate(String), setDescriptionText() must be called
     * after calling this method for the new template text to take effect.
     */
    void setDescriptionTextTemplate(String template) {
        mDescription.setTemplate(template);
    }

    /**
     * @see TemplatePreservingTextView#setText(CharSequence).
     */
    void setDescriptionText(CharSequence text) {
        mDescription.setText(text);
    }

    /**
     * Set action text for the action button.
     * @param actionText Text to be displayed.
     */
    void setActionText(String actionText) {
        mActionButton.setText(actionText);
    }

    /**
     * Set icon drawable.
     * @param iconDrawable Drawable to be shown.
     */
    void setIcon(Drawable iconDrawable) {
        mIcon.setImageDrawable(iconDrawable);
    }

    /**
     * Set click listener for the action button.
     * @param listener {@link android.view.View.OnClickListener} for the action button.
     */
    void setActionButtonOnClickListener(OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    /**
     * Set content description for dismiss button.
     * @param description The content description.
     */
    void setDismissButtonContentDescription(String description) {
        mCloseButton.setContentDescription(description);
    }

    /**
     * Set {@link android.view.View.OnClickListener} for dismiss button.
     * @param listener {@link android.view.View.OnClickListener} to set.
     */
    void setDismissButtonOnClickListener(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }
}
