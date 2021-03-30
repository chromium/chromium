// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.DONE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.DOWN_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.UP_BUTTON_CALLBACK;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Bitmap;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.TranslateAnimation;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.share.screenshot.EditorScreenshotSource;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * LongScreenshotsMediator is responsible for retrieving the long screenshot Bitmaps via
 * {@link LongScreenshotsEntryManager} and displaying them in the area selection dialog.
 */
public class LongScreenshotsMediator
        implements LongScreenshotsEntry.EntryListener, EditorScreenshotSource {
    private Dialog mDialog;
    private boolean mDone;
    private Runnable mDoneCallback;
    private PropertyModel mModel;
    private View mDialogView;
    private final Activity mActivity;
    private final EntryManager mEntryManager;
    private Bitmap mInitialBitmap;
    private LongScreenshotsEntry mCurrentEntry;
    private LongScreenshotsEntry mPendingEntry;
    private int mAnimationsComplete;

    public LongScreenshotsMediator(Activity activity, EntryManager entryManager) {
        mActivity = activity;
        mEntryManager = entryManager;
        mCurrentEntry = mEntryManager.generateInitialEntry();
        mAnimationsComplete = 0;
    }

    private void displayInitialScreenshot() {
        LongScreenshotsEntry entry = mEntryManager.generateInitialEntry();
        entry.setListener(new LongScreenshotsEntry.EntryListener() {
            @Override
            public void onResult(@EntryStatus int status) {
                if (status == EntryStatus.BITMAP_GENERATED) {
                    showAreaSelectionDialog(entry.getBitmap());
                } else {
                    // TODO(tgupta/kmilka): Handle the error case correctly.
                }
            }
        });
    }

    public void showAreaSelectionDialog(Bitmap bitmap) {
        mInitialBitmap = bitmap;
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

        ImageView imageView = mDialogView.findViewById(R.id.screenshot_image);
        imageView.setImageBitmap(mInitialBitmap);

        mDialog.show();
    }

    public void areaSelectionDone(View view) {
        // TODO(1163193): Delete all bitmaps.
        mDialog.cancel();
        mDone = true;
        if (mDoneCallback != null) {
            mDoneCallback.run();
        }
        mDoneCallback = null;
    }

    public void areaSelectionClose(View view) {
        // TODO(1163193): Delete all bitmaps.
        mDialog.cancel();
    }

    public void areaSelectionDown(View view) {
        // TODO(crbug/1153969): Handle a pending entry more gracefully?
        // TODO(crbug/1153969): Handle when already at the bottom of the page.
        if (mPendingEntry != null) {
            return;
        }

        LongScreenshotsEntry newEntry = mEntryManager.getNextEntry(mCurrentEntry.getId());
        if (newEntry == null) {
            return;
        } else if (newEntry.getStatus() == EntryStatus.BOUNDS_BELOW_CAPTURE) {
            // TODO(crbug/1153969): Disable the down button and show a toast
            return;
        }

        mPendingEntry = newEntry;
        // Next entry is already generated/available.
        if (mPendingEntry.getStatus() == EntryStatus.BITMAP_GENERATED) {
            onResult(EntryStatus.BITMAP_GENERATED);
        } else {
            mPendingEntry.setListener(this);
        }
    }

    public void areaSelectionUp(View view) {
        // TODO(crbug/1153969): Handle a pending entry more gracefully
        // TODO(crbug/1153969): Handle when already at the top of the page.
        if (mPendingEntry != null) {
            return;
        }

        LongScreenshotsEntry newEntry = mEntryManager.getPreviousEntry(mCurrentEntry.getId());
        if (newEntry == null) {
            return;
        } else if (newEntry.getStatus() == EntryStatus.BOUNDS_ABOVE_CAPTURE) {
            // TODO(crbug/1153969): Disable the down button and show a toast
            return;
        }

        mPendingEntry = newEntry;
        // Next entry is already generated/available.
        if (mPendingEntry.getStatus() == EntryStatus.BITMAP_GENERATED) {
            onResult(EntryStatus.BITMAP_GENERATED);
        } else {
            mPendingEntry.setListener(this);
        }
    }

    @Override
    public void onResult(@LongScreenshotsEntry.EntryStatus int status) {
        if (status == EntryStatus.BITMAP_GENERATED) {
            ImageView imageView = mDialogView.findViewById(R.id.screenshot_image);
            ImageView nextImageView = mDialogView.findViewById(R.id.next_screenshot_image);
            TranslateAnimation imageAnimation;
            TranslateAnimation nextImageAnimation;

            // Direction of animations is determined by the relative values of the two ids.
            if (mCurrentEntry.getId() < mPendingEntry.getId()) {
                imageAnimation = new TranslateAnimation(0, 0, 0, -imageView.getHeight());
                nextImageAnimation = new TranslateAnimation(0, 0, imageView.getHeight(), 0);
            } else {
                imageAnimation = new TranslateAnimation(0, 0, 0, imageView.getHeight());
                nextImageAnimation = new TranslateAnimation(0, 0, -imageView.getHeight(), 0);
            }

            Bitmap bitmap = mPendingEntry.getBitmap();
            nextImageView.setImageBitmap(bitmap);

            imageAnimation.setDuration(750);
            imageAnimation.setFillAfter(true);
            nextImageAnimation.setDuration(750);
            nextImageAnimation.setFillAfter(true);

            imageView.startAnimation(imageAnimation);
            nextImageView.setVisibility(View.VISIBLE);
            nextImageView.startAnimation(nextImageAnimation);

            // Only trigger next logic after both animations complete.
            mAnimationsComplete = 0;
            imageAnimation.setAnimationListener(new AnimationListener() {
                @Override
                public void onAnimationEnd(Animation animation) {
                    ++mAnimationsComplete;
                    finishAnimation();
                }

                @Override
                public void onAnimationStart(Animation animation) {}

                @Override
                public void onAnimationRepeat(Animation animation) {}
            });
            nextImageAnimation.setAnimationListener(new AnimationListener() {
                @Override
                public void onAnimationEnd(Animation animation) {
                    ++mAnimationsComplete;
                    finishAnimation();
                }

                @Override
                public void onAnimationStart(Animation animation) {}

                @Override
                public void onAnimationRepeat(Animation animation) {}
            });
        } else {
            // TODO(crbug/1153969): Handle non-success cases
        }
    }

    private void finishAnimation() {
        if (mAnimationsComplete < 2) return;

        mCurrentEntry = mPendingEntry;
        mPendingEntry = null;
        mAnimationsComplete = 0;

        ImageView nextImageView = mDialogView.findViewById(R.id.next_screenshot_image);
        ImageView imageView = mDialogView.findViewById(R.id.screenshot_image);

        imageView.setImageBitmap(mCurrentEntry.getBitmap());
        imageView.clearAnimation();
        nextImageView.clearAnimation();
        nextImageView.setVisibility(View.GONE);
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
        // TODO(skare): Populate with actual selected region.
        return mInitialBitmap;
    }
}
