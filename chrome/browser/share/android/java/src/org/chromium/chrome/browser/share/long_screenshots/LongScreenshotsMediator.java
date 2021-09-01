// Copyright 2021 The Chromium Authors. All rights reserved.
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
import android.graphics.Bitmap;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.share.screenshot.EditorScreenshotSource;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * LongScreenshotsMediator is responsible for retrieving the long screenshot Bitmaps and displaying
 * them in the area selection dialog.
 */
public class LongScreenshotsMediator implements LongScreenshotsEntry.EntryListener,
                                                EditorScreenshotSource, View.OnTouchListener {
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

    // Variables for tracking drag action.
    private int mDragStartEventY;
    private int mDragStartViewHeight;

    // Amount by which tapping up/down scrolls the viewport.
    private static final int BUTTON_SCROLL_STEP_PX = 500;
    // Minimum selectable screenshot, vertical size.
    private static final int MINIMUM_VERTICAL_SELECTION_PX = 50;

    private static final String TAG = "long_screenshots";

    public LongScreenshotsMediator(Activity activity, EntryManager entryManager) {
        mActivity = activity;
        mEntryManager = entryManager;
    }

    private void displayInitialScreenshot() {
        // TODO(skare): If testing does not hit memory limits, simplify EntryManager.
        LongScreenshotsEntry entry = mEntryManager.generateInitialEntry();
        entry.setListener(new LongScreenshotsEntry.EntryListener() {
            @Override
            public void onResult(@EntryStatus int status) {
                if (status == EntryStatus.BITMAP_GENERATED) {
                    showAreaSelectionDialog(entry.getBitmap());
                    return;
                }

                if (status == EntryStatus.BITMAP_GENERATION_IN_PROGRESS) {
                    return;
                }

                Toast.makeText(mActivity, R.string.sharing_long_screenshot_unknown_error,
                             Toast.LENGTH_LONG)
                        .show();
            }
        });
    }

    public void showAreaSelectionDialog(Bitmap bitmap) {
        mFullBitmap = bitmap;
        mDialogView = mActivity.getLayoutInflater().inflate(
                R.layout.long_screenshots_area_selection_dialog, null);
        mModel = LongScreenshotsAreaSelectionDialogProperties.defaultModelBuilder()
                         .with(DONE_BUTTON_CALLBACK, this::areaSelectionDone)
                         .with(CLOSE_BUTTON_CALLBACK, this::areaSelectionClose)
                         .with(DOWN_BUTTON_CALLBACK, this::areaSelectionDown)
                         .with(UP_BUTTON_CALLBACK, this::areaSelectionUp)
                         .build();

        PropertyModelChangeProcessor.create(
                mModel, mDialogView, LongScreenshotsAreaSelectionDialogViewBinder::bind);

        mDialog = new Dialog(mActivity, R.style.Theme_Chromium_Fullscreen);
        mDialog.addContentView(mDialogView,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
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
        mImageView.setImageBitmap(mFullBitmap);

        // Start bottom mask in visible viewport.
        ViewGroup.LayoutParams bottomParams = mBottomAreaMaskView.getLayoutParams();
        // TODO(crbug.com/1244430): fix margins and flip this back:
        // bottomParams.height = mFullBitmap.getHeight() - mScrollView.getHeight() - getTopMaskY();
        bottomParams.height = mFullBitmap.getHeight() - 800;
        mBottomAreaMaskView.setLayoutParams(bottomParams);

        LongScreenshotsMetrics.logLongScreenshotsEvent(
                LongScreenshotsMetrics.LongScreenshotsEvent.DIALOG_OPEN);
        mDialog.show();
    }

    public void areaSelectionDone(View view) {
        // TODO(1163193): Delete all bitmaps.
        LongScreenshotsMetrics.logLongScreenshotsEvent(
                LongScreenshotsMetrics.LongScreenshotsEvent.DIALOG_OK);
        mDialog.cancel();
        mDone = true;
        if (mDoneCallback != null) {
            mDoneCallback.run();
        }
        mDoneCallback = null;
    }

    public void areaSelectionClose(View view) {
        LongScreenshotsMetrics.logLongScreenshotsEvent(
                LongScreenshotsMetrics.LongScreenshotsEvent.DIALOG_CANCEL);
        mDialog.cancel();
    }

    private int getTopMaskY() {
        return mTopAreaMaskView.getHeight();
    }

    private int getBottomMaskY() {
        return mFullBitmap.getHeight() - mBottomAreaMaskView.getHeight();
    }

    // User tapped the down button inside the top mask selector.
    public void areaSelectionDown(View view) {
        int oldY = getTopMaskY();
        int newY = oldY + BUTTON_SCROLL_STEP_PX;
        newY = Math.min(newY, getBottomMaskY() - MINIMUM_VERTICAL_SELECTION_PX);

        ViewGroup.LayoutParams params = mTopAreaMaskView.getLayoutParams();
        params.height = newY;
        mTopAreaMaskView.setLayoutParams(params);
        mScrollView.smoothScrollBy(0, newY - oldY);
    }

    // User tapped the up button inside the bottom mask selector.
    public void areaSelectionUp(View view) {
        int oldY = getBottomMaskY();
        int newY = oldY - BUTTON_SCROLL_STEP_PX;
        newY = Math.max(newY, getTopMaskY() + MINIMUM_VERTICAL_SELECTION_PX);

        ViewGroup.LayoutParams params = mBottomAreaMaskView.getLayoutParams();
        params.height = newY;
        mBottomAreaMaskView.setLayoutParams(params);
        mScrollView.smoothScrollBy(0, newY - oldY);
    }

    // Performs postprocessing or error handling on new entry availability.
    // TODO(skare): Remove if no longer needed.
    private void processNewEntry(LongScreenshotsEntry newEntry) {
        if (newEntry == null) {
            return;
        }
        if (newEntry.getStatus() == EntryStatus.BOUNDS_ABOVE_CAPTURE) {
            Toast.makeText(
                         mActivity, R.string.sharing_long_screenshot_reached_top, Toast.LENGTH_LONG)
                    .show();
            return;
        }

        if (newEntry.getStatus() == EntryStatus.BOUNDS_BELOW_CAPTURE) {
            Toast.makeText(mActivity, R.string.sharing_long_screenshot_reached_bottom,
                         Toast.LENGTH_LONG)
                    .show();
            return;
        }

        if (newEntry.getStatus() == EntryStatus.INSUFFICIENT_MEMORY) {
            Log.w(TAG, "Encountered memory pressure.");
            Toast.makeText(mActivity, R.string.sharing_long_screenshot_memory_pressure,
                         Toast.LENGTH_LONG)
                    .show();
            return;
        }
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

    @Override
    public Bitmap getScreenshot() {
        // Extract bitmap data from the bottom of the top mask to the top of the bottom mask.
        int startY = getTopMaskY();
        int endY = getBottomMaskY();

        // Short pages (completely above the fold) may be offset vertically inside the scrollview.
        if (mFullBitmap.getHeight() < mScrollView.getHeight()) {
            int margin = (int) ((mScrollView.getHeight() - mFullBitmap.getHeight()) / 2);
            startY -= margin;
            endY -= margin;
            startY = Math.max(0, startY);
            endY = Math.max(startY + MINIMUM_VERTICAL_SELECTION_PX, endY);
        }

        return Bitmap.createBitmap(mFullBitmap, 0, startY, mFullBitmap.getWidth(), endY - startY);
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

                // Prevent mask regions from overlapping.
                int topMaskY = getTopMaskY();
                int bottomMaskY = getBottomMaskY();
                int bitmapHeight = mFullBitmap.getHeight();
                if (isTop && params.height + MINIMUM_VERTICAL_SELECTION_PX > bottomMaskY) {
                    params.height = bottomMaskY - MINIMUM_VERTICAL_SELECTION_PX;
                }
                if (!isTop
                        && params.height
                                > bitmapHeight - topMaskY - MINIMUM_VERTICAL_SELECTION_PX) {
                    params.height = bitmapHeight - topMaskY - MINIMUM_VERTICAL_SELECTION_PX;
                }

                maskView.setLayoutParams(params);
                handled = true;
                break;
            default:
                break;
        }

        return handled;
    }
}
