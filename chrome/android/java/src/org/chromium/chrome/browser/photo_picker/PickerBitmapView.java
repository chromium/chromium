// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.widget.ImageViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.Animation;
import android.view.animation.ScaleAnimation;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.util.List;

/**
 * A container class for a view showing a photo in the Photo Picker.
 */
public class PickerBitmapView extends SelectableItemView<PickerBitmap> {
    // The length of the image selection animation (in ms).
    private static final int ANIMATION_DURATION = 100;

    // The length of the fade in animation (in ms).
    private static final int IMAGE_FADE_IN_DURATION = 200;

    // The length of the image frame display (in ms).
    private static final int IMAGE_FRAME_DISPLAY = 250;

    // Our context.
    private Context mContext;

    // Our parent category.
    private PickerCategoryView mCategoryView;

    // Our selection delegate.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The request details (meta-data) for the bitmap shown.
    private PickerBitmap mBitmapDetails;

    // The image view containing the bitmap.
    private ImageView mIconView;

    // For video tiles, this lists the duration of the video. Blank for other types.
    private TextView mVideoDuration;

    // The Play button in the bottom right corner. Only shown for videos.
    private ImageView mPlayButton;

    // The little shader in the top left corner (provides backdrop for selection ring on
    // unfavorable image backgrounds).
    private ImageView mScrim;

    // The control that signifies the image has been selected.
    private ImageView mSelectedView;

    // The control that signifies the image has not been selected.
    private ImageView mUnselectedView;

    // The camera/gallery special tile (with icon as drawable).
    private View mSpecialTile;

    // The camera/gallery icon.
    public ImageView mSpecialTileIcon;

    // The label under the special tile.
    public TextView mSpecialTileLabel;

    // Whether the image has been loaded already.
    private boolean mImageLoaded;

    // The amount to use for the border.
    private int mBorder;

    /**
     * Constructor for inflating from XML.
     */
    public PickerBitmapView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = (ImageView) findViewById(R.id.bitmap_view);
        mScrim = (ImageView) findViewById(R.id.scrim);
        mSelectedView = (ImageView) findViewById(R.id.selected);
        mUnselectedView = (ImageView) findViewById(R.id.unselected);
        mSpecialTile = findViewById(R.id.special_tile);
        mSpecialTileIcon = (ImageView) findViewById(R.id.special_tile_icon);
        mSpecialTileLabel = (TextView) findViewById(R.id.special_tile_label);

        // Specific UI controls for video support.
        mVideoDuration = (TextView) findViewById(R.id.video_duration);
        mPlayButton = findViewById(R.id.play_video);
        mPlayButton.setOnClickListener(this);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mCategoryView == null) return;

        int width = mCategoryView.getImageSize();
        int height = mCategoryView.getImageSize();
        setMeasuredDimension(width, height);
    }

    @Override
    public final void onClick(View view) {
        if (view == mPlayButton) {
            mCategoryView.playVideo(mBitmapDetails.getUri());
        } else {
            super.onClick(view);
        }
    }

    @Override
    public void onClick() {
        if (mBitmapDetails == null) {
            return; // Clicks are disabled until initialize() has been called.
        }

        if (isGalleryTile()) {
            mCategoryView.showGallery();
            return;
        } else if (isCameraTile()) {
            mCategoryView.showCamera();
            return;
        }

        // The SelectableItemView expects long press to be the selection event, but this class wants
        // that to happen on click instead.
        onLongClick(this);
    }

    @Override
    protected boolean toggleSelectionForItem(PickerBitmap item) {
        if (isGalleryTile() || isCameraTile()) return false;
        return super.toggleSelectionForItem(item);
    }

    @Override
    public void setChecked(boolean checked) {
        if (!isPictureTile()) {
            return;
        }

        super.setChecked(checked);
        updateSelectionState();
    }

    @Override
    public void onSelectionStateChange(List<PickerBitmap> selectedItems) {
        // If the user cancels the dialog before this object has initialized,
        // the SelectionDelegate will try to notify us that all selections have
        // been cleared. However, we don't need to process that message and, in
        // fact, we can't do so because isPictureTile relies on mBitmapDetails
        // being initialized.
        if (mBitmapDetails == null) return;

        super.onSelectionStateChange(selectedItems);

        updateSelectionState();

        if (!isPictureTile()) return;

        boolean selected = selectedItems.contains(mBitmapDetails);
        boolean checked = super.isChecked();

        boolean needsResize = selected != checked;
        int size = selected && !checked ? mCategoryView.getImageSize() - 2 * mBorder
                                        : mCategoryView.getImageSize();
        if (needsResize) {
            float start;
            float end;
            float videoDurationOffsetX;
            float videoDurationOffsetY;
            if (size != mCategoryView.getImageSize()) {
                start = 1f;
                end = 0.8f;

                float pixels = getResources().getDimensionPixelSize(
                        R.dimen.photo_picker_video_duration_offset);
                videoDurationOffsetX = -pixels;
                videoDurationOffsetY = pixels;
            } else {
                start = 0.8f;
                end = 1f;

                videoDurationOffsetX = 0;
                videoDurationOffsetY = 0;
            }

            Animation animation = new ScaleAnimation(
                    start, end, // Values for x axis.
                    start, end, // Values for y axis.
                    Animation.RELATIVE_TO_SELF, 0.5f, // Pivot X-axis type and value.
                    Animation.RELATIVE_TO_SELF, 0.5f); // Pivot Y-axis type and value.
            animation.setDuration(ANIMATION_DURATION);
            animation.setFillAfter(true); // Keep the results of the animation.
            mIconView.startAnimation(animation);

            ObjectAnimator videoDurationX = ObjectAnimator.ofFloat(
                    mVideoDuration, View.TRANSLATION_X, videoDurationOffsetX);
            ObjectAnimator videoDurationY = ObjectAnimator.ofFloat(
                    mVideoDuration, View.TRANSLATION_Y, videoDurationOffsetY);
            AnimatorSet animatorSet = new AnimatorSet();
            animatorSet.playTogether(videoDurationX, videoDurationY);
            animatorSet.setDuration(ANIMATION_DURATION);
            animatorSet.start();
        }
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        if (!isPictureTile()) return;

        info.setCheckable(true);
        info.setChecked(isChecked());
        CharSequence text = mBitmapDetails.getFilenameWithoutExtension() + " "
                + mBitmapDetails.getLastModifiedString();
        info.setText(text);
    }

    /**
     * Sets the {@link PickerCategoryView} for this PickerBitmapView.
     * @param categoryView The category view showing the images. Used to access
     *     common functionality and sizes and retrieve the {@link SelectionDelegate}.
     */
    public void setCategoryView(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
        mSelectionDelegate = mCategoryView.getSelectionDelegate();
        setSelectionDelegate(mSelectionDelegate);

        mBorder = (int) getResources().getDimension(R.dimen.photo_picker_selected_padding);
    }

    /**
     * Completes the initialization of the PickerBitmapView. Must be called before the image can
     * respond to click events.
     * @param bitmapDetails The details about the bitmap represented by this PickerBitmapView.
     * @param thumbnails The Bitmaps to use for the thumbnail (or null).
     * @param videoDuration The time-length of the video (human-friendly string).
     * @param placeholder Whether the image given is a placeholder or the actual image.
     */
    public void initialize(PickerBitmap bitmapDetails, @Nullable List<Bitmap> thumbnails,
            String videoDuration, boolean placeholder) {
        resetTile();

        mBitmapDetails = bitmapDetails;
        setItem(bitmapDetails);
        if (isCameraTile() || isGalleryTile()) {
            initializeSpecialTile(mBitmapDetails);
            mImageLoaded = true;
        } else {
            setThumbnailBitmap(thumbnails, videoDuration);
            mImageLoaded = !placeholder;
        }

        updateSelectionState();
    }

    /**
     * Initialization for the special tiles (camera/gallery icon).
     * @param bitmapDetails The details about the bitmap represented by this PickerBitmapView.
     */
    public void initializeSpecialTile(PickerBitmap bitmapDetails) {
        int labelStringId = 0;
        Drawable image = null;
        Resources resources = mContext.getResources();

        if (isCameraTile()) {
            image = VectorDrawableCompat.create(
                    resources, R.drawable.ic_photo_camera_grey, mContext.getTheme());
            labelStringId = R.string.photo_picker_camera;
        } else if (isGalleryTile()) {
            image = VectorDrawableCompat.create(
                    resources, R.drawable.ic_collections_grey, mContext.getTheme());
            labelStringId = R.string.photo_picker_browse;
        } else {
            assert false;
        }

        mSpecialTileIcon.setImageDrawable(image);
        ApiCompatibilityUtils.setImageTintList(mSpecialTileIcon,
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_secondary_list));
        ImageViewCompat.setImageTintMode(mSpecialTileIcon, PorterDuff.Mode.SRC_IN);
        mSpecialTileLabel.setText(labelStringId);

        // Reset visibility, since #initialize() sets mSpecialTile visibility to GONE.
        mSpecialTile.setVisibility(View.VISIBLE);
        mSpecialTileIcon.setVisibility(View.VISIBLE);
        mSpecialTileLabel.setVisibility(View.VISIBLE);
    }

    /**
     * Sets a thumbnail bitmap for the current view and ensures the selection border is showing, if
     * the image has already been selected.
     * @param thumbnails The Bitmaps to use for the icon ImageView.
     * @param videoDuration The time-length of the video (human-friendly string).
     * @return True if no image was loaded before (e.g. not even a low-res image).
     */
    public boolean setThumbnailBitmap(List<Bitmap> thumbnails, String videoDuration) {
        assert thumbnails == null || thumbnails.size() > 0;
        if (videoDuration == null) {
            mIconView.setImageBitmap(thumbnails == null ? null : thumbnails.get(0));
        } else {
            mVideoDuration.setText(videoDuration);
            final AnimationDrawable animationDrawable = new AnimationDrawable();
            for (int i = 0; i < thumbnails.size(); ++i) {
                animationDrawable.addFrame(
                        new BitmapDrawable(thumbnails.get(i)), IMAGE_FRAME_DISPLAY);
            }
            animationDrawable.setOneShot(false);
            mIconView.setImageDrawable(animationDrawable);
            animationDrawable.start();
        }

        // If the tile has been selected before the bitmap has loaded, make sure it shows up with
        // a selection border on load.
        if (super.isChecked()) {
            mIconView.getLayoutParams().height = imageSizeWithBorders();
            mIconView.getLayoutParams().width = imageSizeWithBorders();
            addPaddingToParent(mIconView, mBorder);
        } else {
            mIconView.getLayoutParams().height = mCategoryView.getImageSize();
            mIconView.getLayoutParams().width = mCategoryView.getImageSize();
        }

        boolean noImageWasLoaded = !mImageLoaded;
        mImageLoaded = true;
        updateSelectionState();

        return noImageWasLoaded;
    }

    /** Returns the size of the image plus the pre-determined border on each side. */
    private int imageSizeWithBorders() {
        return mCategoryView.getImageSize() - 2 * mBorder;
    }

    /**
     * Initiates fading in of the thumbnail. Note, this should not be called if a grainy version of
     * the thumbnail was loaded from cache. Otherwise a flash will appear.
     */
    public void fadeInThumbnail() {
        mIconView.setAlpha(0.0f);
        mIconView.animate().alpha(1.0f).setDuration(IMAGE_FADE_IN_DURATION).start();
    }

    /**
     * Resets the view to its starting state, which is necessary when the view is about to be
     * re-used.
     */
    private void resetTile() {
        mIconView.setImageBitmap(null);
        mVideoDuration.setText("");
        mUnselectedView.setVisibility(View.GONE);
        mSelectedView.setVisibility(View.GONE);
        mScrim.setVisibility(View.GONE);
        mSpecialTile.setVisibility(View.GONE);
        mSpecialTileIcon.setVisibility(View.GONE);
        mSpecialTileLabel.setVisibility(View.GONE);
    }

    /**
     * Adds padding to the parent of the |view|.
     * @param view The child view of the view to receive the padding.
     * @param padding The amount of padding to use (in pixels).
     */
    private static void addPaddingToParent(View view, int padding) {
        ViewGroup layout = (ViewGroup) view.getParent();
        layout.setPadding(padding, padding, padding, padding);
        layout.requestLayout();
    }

    /**
     * Updates the selection controls for this view.
     */
    private void updateSelectionState() {
        boolean special = !isPictureTile();
        boolean checked = super.isChecked();
        boolean anySelection =
                mSelectionDelegate != null && mSelectionDelegate.isSelectionEnabled();
        Resources resources = mContext.getResources();
        int bgColorId;
        if (!special) {
            bgColorId = R.color.photo_picker_tile_bg_color;
        } else {
            bgColorId = R.color.photo_picker_special_tile_bg_color;
            mSpecialTileLabel.setEnabled(!anySelection);
            mSpecialTileIcon.setEnabled(!anySelection);
            setEnabled(!anySelection);
        }

        setBackgroundColor(ApiCompatibilityUtils.getColor(resources, bgColorId));

        // The visibility of the unselected toggle for multi-selection mode is a little more complex
        // because we don't want to show it when nothing is selected and also not on a blank canvas.
        mSelectedView.setVisibility(!special && checked ? View.VISIBLE : View.GONE);
        boolean showUnselectedToggle = !special && !checked && anySelection && mImageLoaded
                && mCategoryView.isMultiSelectAllowed();
        mUnselectedView.setVisibility(showUnselectedToggle ? View.VISIBLE : View.GONE);
        mScrim.setVisibility(showUnselectedToggle ? View.VISIBLE : View.GONE);
        mPlayButton.setVisibility(
                mImageLoaded && mBitmapDetails.type() == PickerBitmap.TileTypes.VIDEO ? View.VISIBLE
                                                                                      : View.GONE);
    }

    private boolean isGalleryTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.GALLERY;
    }

    private boolean isCameraTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.CAMERA;
    }

    private boolean isPictureTile() {
        return mBitmapDetails.type() == PickerBitmap.TileTypes.PICTURE
                || mBitmapDetails.type() == PickerBitmap.TileTypes.VIDEO;
    }
}
