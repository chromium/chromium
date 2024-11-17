// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.DONE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.DOWN_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.UP_BUTTON_CALLBACK;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.util.Size;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.share.screenshot.EditorScreenshotSource;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * LongScreenshotsMediator is responsible for retrieving the long screenshot Bitmaps and displaying
 * them in the area selection dialog.
 */
public class LongScreenshotsMediator
        implements LongScreenshotsEntry.EntryListener,
                EditorScreenshotSource,
                View.OnTouchListener,
                DialogInterface.OnShowListener {
    private Dialog mDialog;
    private boolean mDone;
    private Runnable mDoneCallback;
    private PropertyModel mModel;
    private View mDialogView;
    private ScrollView mScrollView;
    private View mTopAreaMaskView;
    private View mBottomAreaMaskView;
    private View mInstructionalTextView;
    private View mUpButton;
    private View mDownButton;
    private ImageView mImageView;
    private final Activity mActivity;
    private final EntryManager mEntryManager;
    private Bitmap mFullBitmap;
    private float mDisplayDensity;

    // Variables for tracking drag action.
    private int mDragStartEventY;
    private int mDragStartViewHeight;
    private boolean mDragIsPossibleClick;

    // Test support
    private boolean mDidScaleForTesting;

    // Amount by which tapping up/down scrolls the viewport.
    private static final int BUTTON_SCROLL_STEP_DP = 100;
    // Minimum selectable screenshot, vertical size.
    private static final int MINIMUM_VERTICAL_SELECTION_DP = 50;
    // Minimum height for mask views; should scale with ImageView margins.
    private static final int MINIMUM_MASK_HEIGHT_DP = 20;

    // Enforce a maximum displayed image size to avoid too-large-[software]-bitmap errors in
    // ImageView/Scrollview pair. Larger values with be scaled.
    // The value comes from Android RecordingCanvas#getPanelFrameSize().
    // This value can also be empirically verified using a manual test for this class (see the test
    // file) in case it changes on future versions of Android.
    private static final long DOWNSCALE_AREA_THRESHOLD_BYTES = 100000000;

    public LongScreenshotsMediator(Activity activity, EntryManager entryManager) {
        mActivity = activity;
        mEntryManager = entryManager;
        mDisplayDensity = activity.getResources().getDisplayMetrics().density;
    }

    @VisibleForTesting
    void displayInitialScreenshot() {
        mDidScaleForTesting = false;
        mEntryManager.addBitmapGeneratorObserver(
                new EntryManager.BitmapGeneratorObserver() {
                    @Override
                    public void onStatusChange(int status) {
                        if (status == EntryStatus.CAPTURE_IN_PROGRESS) return;

                        if (status != EntryStatus.CAPTURE_COMPLETE) {
                            mEntryManager.removeBitmapGeneratorObserver(this);
                        }
                    }

                    @Override
                    public void onCompositorReady(Size size, Point offset) {
                        mEntryManager.removeBitmapGeneratorObserver(this);
                        LongScreenshotsEntry entry = mEntryManager.generateFullpageEntry();
                        entry.setListener((status) -> onEntry(entry, status));
                    }
                });
    }

    private void onEntry(LongScreenshotsEntry entry, @EntryStatus int status) {
        if (status == EntryStatus.BITMAP_GENERATION_IN_PROGRESS) {
            return;
        }
        if (status != EntryStatus.BITMAP_GENERATED) {
            Toast.makeText(
                            mActivity,
                            R.string.sharing_long_screenshot_unknown_error,
                            Toast.LENGTH_LONG)
                    .show();
            return;
        }
        Bitmap entryBitmap = entry.getBitmap();
        long bitmapByteCount = entryBitmap.getAllocationByteCount();
        // Scale down the bitmap if passing it to
        // ImageView.setImageBitmap() would throw a too-large
        // error due to OOM (out of memory).
        // TODO(http://crbug.com/1275758): We could include this
        // logic inside the generator and reuse mScaleFactor
        // there.
        if (bitmapByteCount >= DOWNSCALE_AREA_THRESHOLD_BYTES) {
            double oversizeRatio = (1.0 * bitmapByteCount / DOWNSCALE_AREA_THRESHOLD_BYTES);
            double scale = Math.sqrt(oversizeRatio);
            showAreaSelectionDialog(
                    Bitmap.createScaledBitmap(
                            entryBitmap,
                            (int) Math.round(entryBitmap.getWidth() / scale),
                            (int) Math.round(entryBitmap.getHeight() / scale),
                            true));
            mDidScaleForTesting = true;
        } else {
            showAreaSelectionDialog(entryBitmap);
        }
    }

    public void showAreaSelectionDialog(Bitmap bitmap) {
        mFullBitmap = bitmap;
        mDialogView =
                mActivity
                        .getLayoutInflater()
                        .inflate(R.layout.long_screenshots_area_selection_dialog, null);
        mModel =
                LongScreenshotsAreaSelectionDialogProperties.defaultModelBuilder()
                        .with(DONE_BUTTON_CALLBACK, this::areaSelectionDone)
                        .with(CLOSE_BUTTON_CALLBACK, this::areaSelectionClose)
                        .with(DOWN_BUTTON_CALLBACK, this::areaSelectionDown)
                        .with(UP_BUTTON_CALLBACK, this::areaSelectionUp)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mDialogView, LongScreenshotsAreaSelectionDialogViewBinder::bind);

        mDialog = new ChromeDialog(mActivity, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mDialog.addContentView(
                mDialogView,
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));

        mScrollView = mDialogView.findViewById(R.id.long_screenshot_scroll_view);

        mTopAreaMaskView = mDialogView.findViewById(R.id.region_selection_top);
        mBottomAreaMaskView = mDialogView.findViewById(R.id.region_selection_bottom);
        mInstructionalTextView =
                mDialogView.findViewById(R.id.sharing_long_screenshot_dialog_instructions);

        mUpButton = mDialogView.findViewById(R.id.up_button);
        mUpButton.setOnTouchListener(this);
        mDownButton = mDialogView.findViewById(R.id.down_button);
        mDownButton.setOnTouchListener(this);

        mImageView = mDialogView.findViewById(R.id.screenshot_image);
        mImageView.setScaleType(ImageView.ScaleType.FIT_START);
        mImageView.setImageBitmap(mFullBitmap);

        mDialog.setOnShowListener(this);
        mDialog.show();
    }

    @Override
    public void onShow(DialogInterface dialog) {
        // Adjust bottom mask selector.
        ViewGroup.LayoutParams bottomParams = mBottomAreaMaskView.getLayoutParams();
        bottomParams.height = mFullBitmap.getHeight() - mScrollView.getHeight() + getTopMaskY();
        mBottomAreaMaskView.setLayoutParams(bottomParams);
    }

    public void areaSelectionDone(View view) {
        mDialog.cancel();
        mDone = true;
        if (mDoneCallback != null) {
            mDoneCallback.run();
        }
        mDoneCallback = null;
    }

    public void areaSelectionClose(View view) {
        mDialog.cancel();
    }

    /**
     * Converts a measurement in dp to px.
     *
     * @param value Input value in dp.
     * @return |value| in px.
     */
    private int dpToPx(int value) {
        return (int) (value * mDisplayDensity + 0.5f);
    }

    private int getTopMaskY() {
        return mTopAreaMaskView.getHeight();
    }

    private int getBottomMaskY() {
        return ((View) mBottomAreaMaskView.getParent()).getHeight()
                - mBottomAreaMaskView.getHeight();
    }

    // User tapped the down button inside the bottom mask selector; expand region downward.
    public void areaSelectionDown(View view) {
        expandScreenshotRegion(false);
    }

    // User tapped the up button inside the top mask selector; expand region upward.
    public void areaSelectionUp(View view) {
        expandScreenshotRegion(true);
    }

    // Helper for areaSelectionDown/areaSelectionUp.
    // Tapping on the buttons shrinks a mask region, expanding the screenshot area.
    private void expandScreenshotRegion(boolean isTop) {
        View maskView = (isTop ? mTopAreaMaskView : mBottomAreaMaskView);
        int oldHeight = maskView.getHeight();

        // Message if we reached the extent of allowable capture.
        int minimumMaskHeight = dpToPx(MINIMUM_MASK_HEIGHT_DP);
        if (oldHeight <= minimumMaskHeight) {
            Toast.makeText(
                            mActivity,
                            (isTop
                                    ? R.string.sharing_long_screenshot_reached_top
                                    : R.string.sharing_long_screenshot_reached_bottom),
                            Toast.LENGTH_LONG)
                    .show();
            return;
        }

        int newHeight = Math.max(minimumMaskHeight, oldHeight - dpToPx(BUTTON_SCROLL_STEP_DP));
        ViewGroup.LayoutParams params = maskView.getLayoutParams();
        params.height = newHeight;
        maskView.setLayoutParams(params);
        mScrollView.smoothScrollBy(0, (isTop ? 1 : -1) * (newHeight - oldHeight));
    }

    @Override
    public void onResult(@LongScreenshotsEntry.EntryStatus int status) {
        if (status == EntryStatus.BITMAP_GENERATED) {
            // TODO(skare): Re-add in larger-sized paging in case of memory issues on low-end
            // devices; otherwise remove generator pagination in general.
        }
    }

    @VisibleForTesting
    public Dialog getDialog() {
        return mDialog;
    }

    boolean getDidScaleForTesting() {
        return mDidScaleForTesting;
    }

    // EditorScreenshotSource implementation.
    @Override
    public void capture(@Nullable Runnable callback) {
        mDoneCallback = callback;
        displayInitialScreenshot();
    }

    @Override
    public boolean isReady() {
        return mDone;
    }

    // Called by host after the dialog is canceled to obtain screenshot data.
    // Invalidates |mFullBitmap|.
    @Override
    public Bitmap getScreenshot() {
        // Extract bitmap data from the bottom of the top mask to the top of the bottom mask.
        int startY = getTopMaskY();
        int endY = getBottomMaskY();

        // Account for ImageView margin inside the view containing the image and the masks.
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mImageView.getLayoutParams();
        startY -= params.topMargin;
        endY -= params.topMargin;

        // Account for the imageview being  zoomed out due to margins.
        int bitmapWidth = mFullBitmap.getWidth();
        int imageViewWidth = mImageView.getWidth();
        if (bitmapWidth > imageViewWidth) {
            float imageScale = 1.0f * bitmapWidth / imageViewWidth;
            startY = (int) (startY * imageScale);
            endY = (int) (endY * imageScale);
        }

        // Ensure in any case we don't crop beyond the bounds of the screenshot.
        startY = Math.max(startY, 0);
        endY = Math.min(endY, mFullBitmap.getHeight() - 1);
        if (endY <= startY) {
            return null;
        }

        Bitmap cropped =
                Bitmap.createBitmap(mFullBitmap, 0, startY, mFullBitmap.getWidth(), endY - startY);
        mFullBitmap = null;
        return cropped;
    }

    // This listener is used to support dragging. Tapping the buttons is an accessible
    // affordance for the same action, which scrolls by a fixed number of pixels.
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouch(View view, MotionEvent motionEvent) {
        // Logic for the two views has substantial overlap, but we must flip/mirror most behaviors.
        boolean isTop = (view == mUpButton);
        View maskView = isTop ? mTopAreaMaskView : mBottomAreaMaskView;

        // Track vertical dragging from the buttons.
        int y = (int) motionEvent.getRawY();
        boolean handled = false;

        ViewGroup.LayoutParams params;
        switch (motionEvent.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                params = maskView.getLayoutParams();
                mDragStartEventY = y;
                mDragStartViewHeight = params.height;
                mDragIsPossibleClick = true;
                handled = true;
                mScrollView.requestDisallowInterceptTouchEvent(true);
                break;
            case MotionEvent.ACTION_MOVE:
                // Hide "Drag to select Long Screenshot" instructional text after first user action.
                mInstructionalTextView.setVisibility(View.INVISIBLE);
                // Update top or bottom mask selector.
                params = maskView.getLayoutParams();
                int deltaY = (isTop ? 1 : -1) * (y - mDragStartEventY);
                params.height = mDragStartViewHeight + deltaY;
                mDragIsPossibleClick = false;

                // Prevent mask regions from overlapping.
                int topMaskY = getTopMaskY();
                int bottomMaskY = getBottomMaskY();
                int layoutHeight = ((View) mBottomAreaMaskView.getParent()).getHeight();
                int minimumVerticalSelectionPx = dpToPx(MINIMUM_VERTICAL_SELECTION_DP);
                // Ensure masks don't overlap and are separated by a minimum distance.
                if (isTop && params.height + minimumVerticalSelectionPx > bottomMaskY) {
                    params.height = bottomMaskY - minimumVerticalSelectionPx;
                }
                if (!isTop
                        && params.height > layoutHeight - topMaskY - minimumVerticalSelectionPx) {
                    params.height = layoutHeight - topMaskY - minimumVerticalSelectionPx;
                }
                // Ensure masks aren't dragged outside the ImageView bounds.
                int minimumMaskHeightPx = dpToPx(MINIMUM_MASK_HEIGHT_DP);
                if (params.height < minimumMaskHeightPx) {
                    params.height = minimumMaskHeightPx;
                }

                maskView.setLayoutParams(params);
                handled = true;
                break;
            case MotionEvent.ACTION_UP:
                if (mDragIsPossibleClick) {
                    View button = (isTop ? mUpButton : mDownButton);
                    button.performClick();
                    mDragIsPossibleClick = false;
                }
                break;
            default:
                break;
        }

        return handled;
    }
}
