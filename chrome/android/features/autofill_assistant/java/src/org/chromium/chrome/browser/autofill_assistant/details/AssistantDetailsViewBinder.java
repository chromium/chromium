// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.media.ThumbnailUtils;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StyleRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Autofill Assistant details view. These
 * updates are pulled from the {@link AssistantDetailsModel} when a notification of an update is
 * received.
 */
class AssistantDetailsViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantDetailsModel,
                AssistantDetailsViewBinder.ViewHolder, PropertyKey> {
    private static final int IMAGE_BORDER_RADIUS = 8;
    private static final int PULSING_DURATION_MS = 1_000;

    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final GradientDrawable mDefaultImage;
        final ImageView mImageView;
        final TextView mTitleView;
        final TextView mDescriptionLine1View;
        final TextView mDescriptionLine2View;
        final TextView mDescriptionLine3View;
        final TextView mPriceAttributionView;
        final View mPriceView;
        final TextView mTotalPriceLabelView;
        final TextView mTotalPriceView;

        ViewHolder(Context context, View detailsView) {
            mDefaultImage = (GradientDrawable) context.getResources().getDrawable(
                    R.drawable.autofill_assistant_default_details);
            mImageView = detailsView.findViewById(R.id.details_image);
            mTitleView = detailsView.findViewById(R.id.details_title);
            mDescriptionLine1View = detailsView.findViewById(R.id.details_line1);
            mDescriptionLine2View = detailsView.findViewById(R.id.details_line2);
            mDescriptionLine3View = detailsView.findViewById(R.id.details_line3);
            mPriceAttributionView = detailsView.findViewById(R.id.details_price_attribution);
            mPriceView = detailsView.findViewById(R.id.details_price);
            mTotalPriceView = detailsView.findViewById(R.id.details_total_price);
            mTotalPriceLabelView = detailsView.findViewById(R.id.details_total_price_label);
        }
    }

    private final Context mContext;

    private final int mImageWidth;
    private final int mImageHeight;
    private final int mPulseAnimationStartColor;
    private final int mPulseAnimationEndColor;

    private ValueAnimator mPulseAnimation;
    private ImageFetcher mImageFetcher;

    AssistantDetailsViewBinder(Context context, ImageFetcher imageFetcher) {
        mContext = context;
        mImageWidth = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mImageHeight = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mPulseAnimationStartColor = context.getResources().getColor(R.color.modern_grey_300);
        mPulseAnimationEndColor = context.getResources().getColor(R.color.modern_grey_200);
        mImageFetcher = imageFetcher;
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    void destroy() {
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    @Override
    public void bind(AssistantDetailsModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantDetailsModel.DETAILS == propertyKey) {
            AssistantDetails details = model.get(AssistantDetailsModel.DETAILS);
            if (details == null) {
                // Handled by the AssistantDetailsCoordinator.
                return;
            }

            setDetails(details, view);
        } else {
            assert false : "Unhandled property detected in AssistantDetailsViewBinder!";
        }
    }

    private void setDetails(AssistantDetails details, ViewHolder viewHolder) {
        viewHolder.mTitleView.setText(details.getTitle());
        viewHolder.mDescriptionLine1View.setText(details.getDescriptionLine1());
        viewHolder.mDescriptionLine2View.setText(details.getDescriptionLine2());
        viewHolder.mDescriptionLine3View.setText(details.getDescriptionLine3());
        viewHolder.mTotalPriceLabelView.setText(details.getTotalPriceLabel());
        viewHolder.mTotalPriceView.setText(details.getTotalPrice());
        viewHolder.mPriceAttributionView.setText(details.getPriceAttribution());

        // Allow title line wrapping according to number of maximum allowed lines.
        if (details.getTitleMaxLines() == 1) {
            viewHolder.mTitleView.setSingleLine(true);
            viewHolder.mTitleView.setEllipsize(null);
        } else {
            viewHolder.mTitleView.setSingleLine(false);
            viewHolder.mTitleView.setMaxLines(details.getTitleMaxLines());
            viewHolder.mTitleView.setEllipsize(TextUtils.TruncateAt.END);
        }

        hideIfEmpty(viewHolder.mDescriptionLine1View);
        hideIfEmpty(viewHolder.mDescriptionLine2View);
        hideIfEmpty(viewHolder.mDescriptionLine3View);
        hideIfEmpty(viewHolder.mPriceAttributionView);

        // If no price provided, hide the price view (containing separator, price label, and price).
        viewHolder.mPriceView.setVisibility(
                details.getTotalPrice().isEmpty() ? View.GONE : View.VISIBLE);

        viewHolder.mImageView.setVisibility(View.VISIBLE);
        if (details.getImageUrl().isEmpty()) {
            if (details.getShowImagePlaceholder()) {
                viewHolder.mImageView.setImageDrawable(viewHolder.mDefaultImage);
                viewHolder.mImageView.setOnClickListener(null);
            } else {
                viewHolder.mImageView.setVisibility(View.GONE);
            }
        } else {
            // Download image and then set it in the view.
            mImageFetcher.fetchImage(details.getImageUrl(),
                    ImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME, image -> {
                        if (image != null) {
                            viewHolder.mImageView.setImageDrawable(getRoundedImage(image));
                            if (details.hasImageClickthroughData()
                                    && details.getImageClickthroughData().getAllowClickthrough()) {
                                viewHolder.mImageView.setOnClickListener(unusedView
                                        -> onImageClicked(mContext, details.getImageUrl(),
                                                details.getImageClickthroughData()));
                            } else {
                                viewHolder.mImageView.setOnClickListener(null);
                            }
                        }
                    });
        }

        setTextStyles(details, viewHolder);
    }

    private void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }

    private void setTextStyles(AssistantDetails details, ViewHolder viewHolder) {
        setTextStyle(viewHolder.mTitleView, details.getUserApprovalRequired(),
                details.getHighlightTitle(), R.style.TextAppearance_AssistantDetailsTitle);
        setTextStyle(viewHolder.mDescriptionLine1View, details.getUserApprovalRequired(),
                details.getHighlightLine1(), R.style.TextAppearance_BlackBody);
        setTextStyle(viewHolder.mDescriptionLine2View, details.getUserApprovalRequired(),
                details.getHighlightLine2(), R.style.TextAppearance_BlackBody);
        setTextStyle(viewHolder.mDescriptionLine3View, details.getUserApprovalRequired(),
                details.getHighlightLine3(), R.style.TextAppearance_AssistantDetailsAttribution);
        setTextStyle(viewHolder.mPriceAttributionView, details.getUserApprovalRequired(),
                details.getHighlightLine3(), R.style.TextAppearance_AssistantDetailsAttribution);
        setTextStyle(viewHolder.mTotalPriceLabelView, details.getUserApprovalRequired(),
                /* highlight= */ false, R.style.TextAppearance_BlackButtonText);
        setTextStyle(viewHolder.mTotalPriceView, details.getUserApprovalRequired(),
                /* highlight= */ false, R.style.TextAppearance_AssistantDetailsPrice);

        if (shouldStartOrContinuePlaceholderAnimation(details, viewHolder)) {
            startOrContinuePlaceholderAnimations(viewHolder);
        } else {
            stopPlaceholderAnimations();
        }
    }

    private boolean shouldStartOrContinuePlaceholderAnimation(
            AssistantDetails details, ViewHolder viewHolder) {
        boolean isAtLeastOneFieldEmpty = viewHolder.mTitleView.length() == 0
                || viewHolder.mDescriptionLine1View.length() == 0
                || viewHolder.mDescriptionLine2View.length() == 0
                || viewHolder.mDescriptionLine3View.length() == 0
                || viewHolder.mImageView.getDrawable() == viewHolder.mDefaultImage;
        return details.getAnimatePlaceholders() && isAtLeastOneFieldEmpty;
    }

    private void setTextStyle(
            TextView view, boolean approvalRequired, boolean highlight, @StyleRes int normalStyle) {
        ApiCompatibilityUtils.setTextAppearance(view, normalStyle);

        if (approvalRequired && highlight) {
            // Emphasized style.
            view.setTypeface(view.getTypeface(), Typeface.BOLD_ITALIC);
        } else if (approvalRequired) {
            // De-emphasized style.
            view.setTextColor(ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.modern_grey_300));
        }
    }

    private Drawable getRoundedImage(Bitmap bitmap) {
        RoundedBitmapDrawable roundedBitmap =
                RoundedBitmapDrawableFactory.create(mContext.getResources(),
                        ThumbnailUtils.extractThumbnail(bitmap, mImageWidth, mImageHeight));
        roundedBitmap.setCornerRadius(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                IMAGE_BORDER_RADIUS, mContext.getResources().getDisplayMetrics()));
        return roundedBitmap;
    }

    private void startOrContinuePlaceholderAnimations(ViewHolder viewHolder) {
        if (mPulseAnimation != null) {
            return;
        }
        mPulseAnimation = ValueAnimator.ofInt(mPulseAnimationStartColor, mPulseAnimationEndColor);
        mPulseAnimation.setDuration(PULSING_DURATION_MS);
        mPulseAnimation.setEvaluator(new ArgbEvaluator());
        mPulseAnimation.setRepeatCount(ValueAnimator.INFINITE);
        mPulseAnimation.setRepeatMode(ValueAnimator.REVERSE);
        mPulseAnimation.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        mPulseAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                viewHolder.mTitleView.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDescriptionLine1View.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDescriptionLine2View.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDescriptionLine3View.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDefaultImage.setColor(Color.TRANSPARENT);
            }
        });
        mPulseAnimation.addUpdateListener(animation -> {
            int animatedValue = (int) animation.getAnimatedValue();
            viewHolder.mTitleView.setBackgroundColor(
                    viewHolder.mTitleView.length() == 0 ? animatedValue : Color.TRANSPARENT);
            viewHolder.mDescriptionLine1View.setBackgroundColor(
                    viewHolder.mDescriptionLine1View.length() == 0 ? animatedValue
                                                                   : Color.TRANSPARENT);
            viewHolder.mDescriptionLine2View.setBackgroundColor(
                    viewHolder.mDescriptionLine2View.length() == 0 ? animatedValue
                                                                   : Color.TRANSPARENT);
            viewHolder.mDescriptionLine3View.setBackgroundColor(
                    viewHolder.mDescriptionLine3View.length() == 0 ? animatedValue
                                                                   : Color.TRANSPARENT);
            viewHolder.mDefaultImage.setColor(
                    viewHolder.mImageView.getDrawable() == viewHolder.mDefaultImage
                            ? animatedValue
                            : Color.TRANSPARENT);
        });
        mPulseAnimation.start();
    }

    private void stopPlaceholderAnimations() {
        if (mPulseAnimation != null) {
            mPulseAnimation.cancel();
            mPulseAnimation = null;
        }
    }

    /**
     * Clicking on the image will trigger a modal dialog asking whether the user wants to
     * see the original image, if they choose to see it, a new custom tab pointing to the
     * url of the orinial image will present on top of current one.
     */
    private void onImageClicked(
            Context context, String imageUrl, ImageClickthroughData clickthroughData) {
        ModalDialogManager manager = new ModalDialogManager(
                new AppModalPresenter((android.app.Activity) context), ModalDialogType.APP);

        // Handles 'View' and 'Cancel' actions from modal dialog.
        ModalDialogProperties.Controller dialogController = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    manager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                } else {
                    manager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    String presentUrl = clickthroughData.getClickthroughUrl().isEmpty()
                            ? imageUrl
                            : clickthroughData.getClickthroughUrl();
                    CustomTabActivity.showInfoPage(context.getApplicationContext(), presentUrl);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };

        Resources resources = context.getResources();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController);
        if (!clickthroughData.getDescription().isEmpty()) {
            builder.with(ModalDialogProperties.MESSAGE, clickthroughData.getDescription());
        } else {
            builder.with(ModalDialogProperties.MESSAGE, resources,
                    R.string.autofill_assistant_view_original_image_desc);
        }

        if (!clickthroughData.getPositiveText().isEmpty()) {
            builder.with(
                    ModalDialogProperties.POSITIVE_BUTTON_TEXT, clickthroughData.getPositiveText());
        } else {
            builder.with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                    R.string.autofill_assistant_view_original_image_view);
        }

        if (!clickthroughData.getNegativeText().isEmpty()) {
            builder.with(
                    ModalDialogProperties.NEGATIVE_BUTTON_TEXT, clickthroughData.getNegativeText());
        } else {
            builder.with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                    R.string.autofill_assistant_view_original_image_cancel);
        }

        PropertyModel dialogModel = builder.build();
        manager.showDialog(dialogModel, ModalDialogType.APP);
    }
}
