// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.DrawableCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.ui.UiUtils;

/** Contains a label that overlays a tab card in the grid tab switcher. */
public class TabCardLabelView extends LinearLayout {
    private static final String TAG = "TabCardLabelView";

    private TabCardLabelData mLastData;
    private TextView mLabelText;
    private AsyncImageView mIconView;

    /** Default {@link LinearLayout} constructor. */
    public TabCardLabelView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.tab_label_icon);
        mLabelText = findViewById(R.id.tab_label_text);
    }

    /** Set the {@link TabCardLabelData} to use. Setting null clears out the view and hides it. */
    void setData(@Nullable TabCardLabelData data) {
        if (mLastData == data) return;

        if (data == null) {
            reset();
        } else {
            setLabelType(data.labelType);
            setAsyncImageFactory(data.asyncImageFactory);
            setTextResolver(data.textResolver);
            setContentDescriptionResolver(data.contentDescriptionResolver);
            setVisibility(View.VISIBLE);
        }
        mLastData = data;
    }

    private void reset() {
        setVisibility(View.GONE);
        setAsyncImageFactory(null);
        mLabelText.setText(null);
        setContentDescriptionResolver(null);
        mIconView.setUnavailableDrawable(null);
        mIconView.setWaitingDrawable(null);
    }

    private void setTextResolver(TextResolver textResolver) {
        mLabelText.setText(textResolver.resolve(getContext()));
    }

    private void setContentDescriptionResolver(@Nullable TextResolver contentDescriptionResolver) {
        @Nullable CharSequence contentDescription = null;
        if (contentDescriptionResolver != null) {
            contentDescription = contentDescriptionResolver.resolve(getContext());
        }
        setContentDescription(contentDescription);
        mLabelText.setImportantForAccessibility(
                contentDescription == null
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                        : View.IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    private void setAsyncImageFactory(@Nullable AsyncImageView.Factory factory) {
        mIconView.setVisibility(factory == null ? View.GONE : View.VISIBLE);
        mIconView.setAsyncImageDrawable(factory, null);
    }

    private void setLabelType(@TabCardLabelType int labelType) {
        if (mLastData != null && labelType == mLastData.labelType) return;

        Context context = getContext();
        if (labelType == TabCardLabelType.ACTIVITY_UPDATE) {
            DrawableCompat.wrap(getBackground())
                    .setTint(MaterialColors.getColor(context, R.attr.colorSecondaryContainer, TAG));
            DrawableCompat.wrap(mIconView.getBackground())
                    .setTint(MaterialColors.getColor(context, R.attr.colorSurface, TAG));
            Drawable drawable =
                    UiUtils.getTintedDrawable(
                            context,
                            R.drawable.ic_group_24dp,
                            R.color.default_icon_color_secondary_tint_list);
            Resources res = context.getResources();
            int inset = res.getDimensionPixelSize(R.dimen.tab_card_label_icon_inset);
            int size = res.getDimensionPixelSize(R.dimen.tab_card_label_icon_size) - 2 * inset;
            drawable.setBounds(0, 0, size, size);
            Drawable insetDrawable = new InsetDrawable(drawable, inset);
            mIconView.setUnavailableDrawable(insetDrawable);
            mIconView.setWaitingDrawable(insetDrawable);
        } else if (labelType == TabCardLabelType.PRICE_DROP) {
            DrawableCompat.wrap(getBackground()).setTint(context.getColor(R.color.green_50));
            mIconView.setUnavailableDrawable(null);
            mIconView.setWaitingDrawable(null);
        } else {
            assert false : "Not reached.";
        }
    }
}
