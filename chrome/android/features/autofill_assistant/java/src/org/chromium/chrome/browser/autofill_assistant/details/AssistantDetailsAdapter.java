// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import static org.chromium.chrome.browser.autofill_assistant.AssistantAccessibilityUtils.setAccessibility;

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
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.StyleRes;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for binding details to their associated view.
 */
class AssistantDetailsAdapter extends RecyclerView.Adapter<AssistantDetailsAdapter.ViewHolder> {
    private final List<AssistantDetails> mDetails = new ArrayList<>();

    private static final int IMAGE_BORDER_RADIUS = 8;
    private static final int PULSING_DURATION_MS = 1_000;

    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder extends RecyclerView.ViewHolder {
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
            super(detailsView);

            mDefaultImage = (GradientDrawable) ResourcesCompat.getDrawable(context.getResources(),
                    R.drawable.autofill_assistant_default_details, /* theme= */ null);
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
    private final int mTextPlaceholdersHeight;
    private final int mTextPlaceholdersMargin;

    private ValueAnimator mPlaceholdersColorAnimation;
    private List<Callback<Integer>> mPlaceholdersColorCallbacks = new ArrayList<>();
    private ImageFetcher mImageFetcher;

    AssistantDetailsAdapter(Context context, ImageFetcher imageFetcher) {
        mContext = context;
        mImageWidth = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mImageHeight = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mPulseAnimationStartColor = context.getResources().getColor(R.color.modern_grey_300);
        mPulseAnimationEndColor = context.getResources().getColor(R.color.modern_grey_200);
        mTextPlaceholdersHeight = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_text_placeholders_height);
        mTextPlaceholdersMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_text_placeholders_margin);
        mImageFetcher = imageFetcher;
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    void destroy() {
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    /** Set the details. */
    void setDetails(List<AssistantDetails> details) {
        // Compute the diff.
        int oldSize = mDetails.size();
        int newSize = details.size();
        DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(new DiffUtil.Callback() {
            @Override
            public int getOldListSize() {
                return oldSize;
            }

            @Override
            public int getNewListSize() {
                return newSize;
            }

            @Override
            public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
                // Assume that oldList[i] == newList[j] only if i == j, to minimize the number of
                // add/remove animations when replacing or appending details.
                return oldItemPosition == newItemPosition;
            }

            @Override
            public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
                // Assume contents are never the same, that way we always update each details view.
                return false;
            }

            @Override
            public Object getChangePayload(int oldItemPosition, int newItemPosition) {
                // We need to return a non-null payload, otherwise the whole details view will
                // fade-out then fade-in when changing.
                return details.get(newItemPosition);
            }
        }, /* detectMoves= */ false);

        // Stop the placeholders animation.
        stopPlaceholderAnimations();

        // Set the details.
        mDetails.clear();
        mDetails.addAll(details);

        // Notify the change.
        diffResult.dispatchUpdatesTo(this);
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        Context context = viewGroup.getContext();
        View detailsView = LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_details, viewGroup, /* attachToRoot= */ false);
        return new ViewHolder(context, detailsView);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int i) {
        bindDetails(mDetails.get(i), viewHolder);
    }

    @Override
    public int getItemCount() {
        return mDetails.size();
    }

    private void bindDetails(AssistantDetails details, ViewHolder viewHolder) {
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mTitleView, details.getTitle(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mDescriptionLine1View, details.getDescriptionLine1(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mDescriptionLine2View, details.getDescriptionLine2(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mDescriptionLine3View, details.getDescriptionLine3(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mTotalPriceLabelView, details.getTotalPriceLabel(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mTotalPriceView, details.getTotalPrice(), null);
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mPriceAttributionView, details.getPriceAttribution(), null);

        AssistantPlaceholdersConfiguration placeholders = details.getPlaceholdersConfiguration();
        boolean hideDescriptionLine1 = details.getDescriptionLine1().isEmpty()
                && !placeholders.getShowDescriptionLine1Placeholder();
        boolean hideDescriptionLine2 = details.getDescriptionLine2().isEmpty()
                && !placeholders.getShowDescriptionLine2Placeholder();
        boolean hideDescriptionLine3 = details.getDescriptionLine3().isEmpty()
                && !placeholders.getShowDescriptionLine3Placeholder();

        // Allow title line wrapping according to number of maximum allowed lines.
        // TODO(crbug.com/806868): Should we move the hide/placeholders/maxLines logic to C++?
        int titleMaxLines = 1;
        if (hideDescriptionLine1) titleMaxLines++;
        if (hideDescriptionLine2) titleMaxLines++;

        if (titleMaxLines == 1) {
            viewHolder.mTitleView.setSingleLine(true);
            viewHolder.mTitleView.setEllipsize(null);
        } else {
            viewHolder.mTitleView.setSingleLine(false);
            viewHolder.mTitleView.setMaxLines(titleMaxLines);
            viewHolder.mTitleView.setEllipsize(TextUtils.TruncateAt.END);
        }

        // Hide views without text or placeholders.
        hideIf(viewHolder.mDescriptionLine1View, hideDescriptionLine1);
        hideIf(viewHolder.mDescriptionLine2View, hideDescriptionLine2);
        hideIf(viewHolder.mDescriptionLine3View, hideDescriptionLine3);
        hideIfEmpty(viewHolder.mPriceAttributionView);

        // Set the height of the potential placeholders next to the image.
        setTextHeightAndMargin(viewHolder.mTitleView, placeholders.getShowTitlePlaceholder());
        setTextHeightAndMargin(viewHolder.mDescriptionLine1View,
                placeholders.getShowDescriptionLine1Placeholder());
        setTextHeightAndMargin(viewHolder.mDescriptionLine2View,
                placeholders.getShowDescriptionLine2Placeholder());
        setTextHeightAndMargin(viewHolder.mDescriptionLine3View,
                placeholders.getShowDescriptionLine3Placeholder());

        // If no price provided, hide the price view (containing separator, price label, and price).
        viewHolder.mPriceView.setVisibility(
                details.getTotalPrice().isEmpty() ? View.GONE : View.VISIBLE);

        viewHolder.mImageView.setVisibility(View.VISIBLE);
        setAccessibility(viewHolder.mImageView, details.getImageAccessibilityHint());

        if (details.getImageUrl().isEmpty()) {
            if (placeholders.getShowImagePlaceholder()) {
                viewHolder.mImageView.setImageDrawable(viewHolder.mDefaultImage);
                viewHolder.mImageView.setOnClickListener(null);
            } else {
                viewHolder.mImageView.setVisibility(View.GONE);
            }
        } else {
            // Download image and then set it in the view.
            ImageFetcher.Params params = ImageFetcher.Params.create(
                    details.getImageUrl(), ImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME);
            mImageFetcher.fetchImage(params, image -> {
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
        hideIf(view, view.length() == 0);
    }

    private void hideIf(TextView view, boolean hide) {
        view.setVisibility(hide ? View.GONE : View.VISIBLE);
    }

    private void setTextHeightAndMargin(TextView view, boolean isPlaceholder) {
        LinearLayout.LayoutParams layoutParams = (LinearLayout.LayoutParams) view.getLayoutParams();

        if (isPlaceholder) {
            layoutParams.height = mTextPlaceholdersHeight;
            layoutParams.topMargin = mTextPlaceholdersMargin;
            layoutParams.bottomMargin = mTextPlaceholdersMargin;
        } else {
            layoutParams.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            layoutParams.topMargin = 0;
            layoutParams.bottomMargin = 0;
        }

        view.setLayoutParams(layoutParams);
    }

    private void setTextStyles(AssistantDetails details, ViewHolder viewHolder) {
        setTextStyle(viewHolder.mTitleView, details.getUserApprovalRequired(),
                details.getHighlightTitle(), R.style.TextAppearance_AssistantDetailsTitle);
        setTextStyle(viewHolder.mDescriptionLine1View, details.getUserApprovalRequired(),
                details.getHighlightLine1(), R.style.TextAppearance_TextMedium_Secondary);
        setTextStyle(viewHolder.mDescriptionLine2View, details.getUserApprovalRequired(),
                details.getHighlightLine2(), R.style.TextAppearance_TextMedium_Secondary);
        // TODO(crbug.com/1118226): Update the styles that use *_Disabled with UX guidance.
        setTextStyle(viewHolder.mDescriptionLine3View, details.getUserApprovalRequired(),
                details.getHighlightLine3(), R.style.TextAppearance_TextSmall_Secondary);
        setTextStyle(viewHolder.mPriceAttributionView, details.getUserApprovalRequired(),
                details.getHighlightLine3(), R.style.TextAppearance_TextSmall_Secondary);
        setTextStyle(viewHolder.mTotalPriceLabelView, details.getUserApprovalRequired(),
                /* highlight= */ false, R.style.TextAppearance_TextMedium_Secondary);
        setTextStyle(viewHolder.mTotalPriceView, details.getUserApprovalRequired(),
                /* highlight= */ false, R.style.TextAppearance_AssistantDetailsPrice);

        if (shouldStartOrContinuePlaceholderAnimation(details.getPlaceholdersConfiguration())) {
            startOrContinuePlaceholderAnimations(details, viewHolder);
        }
    }

    private boolean shouldStartOrContinuePlaceholderAnimation(
            AssistantPlaceholdersConfiguration placeholders) {
        return placeholders.getShowImagePlaceholder() || placeholders.getShowTitlePlaceholder()
                || placeholders.getShowDescriptionLine1Placeholder()
                || placeholders.getShowDescriptionLine2Placeholder()
                || placeholders.getShowDescriptionLine3Placeholder();
    }

    private void setTextStyle(
            TextView view, boolean approvalRequired, boolean highlight, @StyleRes int normalStyle) {
        ApiCompatibilityUtils.setTextAppearance(view, normalStyle);

        if (approvalRequired && highlight) {
            // Emphasized style.
            view.setTypeface(view.getTypeface(), Typeface.BOLD_ITALIC);
        } else if (approvalRequired) {
            // De-emphasized style.
            // TODO(b/154592651) Use setTextAppearance instead of setTextColor.
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

    private void startOrContinuePlaceholderAnimations(
            AssistantDetails details, ViewHolder viewHolder) {
        // Start the placeholders color animation if necessary.
        if (mPlaceholdersColorAnimation == null) {
            mPlaceholdersColorAnimation =
                    ValueAnimator.ofInt(mPulseAnimationStartColor, mPulseAnimationEndColor);
            mPlaceholdersColorAnimation.setDuration(PULSING_DURATION_MS);
            mPlaceholdersColorAnimation.setEvaluator(new ArgbEvaluator());
            mPlaceholdersColorAnimation.setRepeatCount(ValueAnimator.INFINITE);
            mPlaceholdersColorAnimation.setRepeatMode(ValueAnimator.REVERSE);
            mPlaceholdersColorAnimation.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
            mPlaceholdersColorAnimation.addUpdateListener(animation -> {
                // Set the color of the placeholders.
                int animatedValue = (int) animation.getAnimatedValue();
                setPlaceholdersColor(animatedValue);
            });
            mPlaceholdersColorAnimation.start();
        }

        // Change the background color of the placeholders when the animated value changes.
        AssistantPlaceholdersConfiguration placeholders = details.getPlaceholdersConfiguration();
        mPlaceholdersColorCallbacks.add(animatedValue -> {
            viewHolder.mTitleView.setBackgroundColor(
                    placeholders.getShowTitlePlaceholder() ? animatedValue : Color.TRANSPARENT);
            viewHolder.mDescriptionLine1View.setBackgroundColor(
                    placeholders.getShowDescriptionLine1Placeholder() ? animatedValue
                                                                      : Color.TRANSPARENT);
            viewHolder.mDescriptionLine2View.setBackgroundColor(
                    placeholders.getShowDescriptionLine2Placeholder() ? animatedValue
                                                                      : Color.TRANSPARENT);
            viewHolder.mDescriptionLine3View.setBackgroundColor(
                    placeholders.getShowDescriptionLine3Placeholder() ? animatedValue
                                                                      : Color.TRANSPARENT);
            viewHolder.mDefaultImage.setColor(
                    placeholders.getShowImagePlaceholder() ? animatedValue : Color.TRANSPARENT);
        });
    }

    private void stopPlaceholderAnimations() {
        setPlaceholdersColor(Color.TRANSPARENT);
        mPlaceholdersColorCallbacks.clear();

        if (mPlaceholdersColorAnimation != null) {
            mPlaceholdersColorAnimation.cancel();
            mPlaceholdersColorAnimation = null;
        }
    }

    private void setPlaceholdersColor(int color) {
        for (Callback<Integer> callback : mPlaceholdersColorCallbacks) {
            callback.onResult(color);
        }
    }

    @VisibleForTesting
    boolean isRunningPlaceholdersAnimation() {
        return mPlaceholdersColorAnimation != null;
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
