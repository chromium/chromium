// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;
import android.content.ComponentName;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Build;
import android.view.View;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.content_creation.reactions.LightweightReactionsMediator.GifGeneratorHost;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.chrome.browser.content_creation.reactions.scene.SceneCoordinator;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarControlsDelegate;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarCoordinator;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.share.BaseScreenshotCoordinator;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.components.content_creation.reactions.ReactionService;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Responsible for reactions main UI and its subcomponents.
 */
public class LightweightReactionsCoordinatorImpl extends BaseScreenshotCoordinator
        implements LightweightReactionsCoordinator, ToolbarControlsDelegate {
    private static final String GIF_MIME_TYPE = "image/gif";

    private final FragmentManager mFragmentManager;
    private final ReactionService mReactionService;
    private final LightweightReactionsMediator mMediator;
    private final LightweightReactionsDialog mDialog;
    private final SceneCoordinator mSceneCoordinator;
    private final WindowAndroid mWindowAndroid;

    private List<ReactionMetadata> mAvailableReactions;
    private Bitmap[] mThumbnails;
    private ToolbarCoordinator mToolbarCoordinator;

    private boolean mScreenshotReady;
    private boolean mAssetsFetched;

    private long mDialogOpenedTime;
    private long mAssetFetchStartTime;
    private long mGenerationStartTime;

    private Toast mToast;

    /**
     * Constructs a new LightweightReactionsCoordinatorImpl which initializes and displays the
     * Lightweight Reactions scene.
     *
     * @param activity The parent activity.
     * @param windowAndroid The {@link WindowAndroid} associated with the screenshot.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param reactionService The {@link ReactionService} to use for Reaction operations.
     */
    public LightweightReactionsCoordinatorImpl(Activity activity, WindowAndroid windowAndroid,
            String shareUrl, ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController, ReactionService reactionService) {
        super(activity, shareUrl, chromeOptionShareCallback, sheetController);
        mWindowAndroid = windowAndroid;
        mScreenshotReady = false;
        mAssetsFetched = false;
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        mReactionService = reactionService;

        ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.DISK_CACHE_ONLY, ProfileKey.getLastUsedRegularProfileKey());
        mMediator = new LightweightReactionsMediator(imageFetcher);

        mDialog = new LightweightReactionsDialog();
        mSceneCoordinator = new SceneCoordinator(activity, mMediator);

        mReactionService.getReactions((reactions) -> {
            assert reactions != null;
            assert reactions.size() > 0;
            mAvailableReactions = reactions;
            mAssetFetchStartTime = System.currentTimeMillis();
            mMediator.fetchAssetsAndGetThumbnails(reactions, this::onAssetsFetched);
        });
    }

    /**
     * Stores the thumbnails of the reactions, then attempts to finish the initialization of the
     * feature.
     * @param thumbnails The list of thumbnails. The order must be the same as
     *                   {@code mAvailableReactions}.
     */
    private void onAssetsFetched(Bitmap[] thumbnails) {
        boolean success = thumbnails != null;
        LightweightReactionsMetrics.recordAssetsFetched(
                success, System.currentTimeMillis() - mAssetFetchStartTime);
        if (success) {
            mAssetsFetched = true;
            mThumbnails = thumbnails;
            maybeFinishInitialization();
        } else {
            showErrorToast(mActivity.getString(R.string.lightweight_reactions_error_asset_fetch));
        }
    }

    /**
     * Initializes the scene and the toolbar, and shows the Lightweight Reactions dialog.
     *
     * <p><b>Note:</b> The screenshot must be ready and the assets must have been fetched. If either
     * is missing or had errors, this is a no-op.
     */
    private void maybeFinishInitialization() {
        if (!mScreenshotReady || !mAssetsFetched) {
            // Wait until both operations have completed.
            return;
        }

        mDialog.init(mScreenshot, mSceneCoordinator, (View view) -> {
            mToolbarCoordinator = new ToolbarCoordinator(view, this, mSceneCoordinator);
            mToolbarCoordinator.initReactions(mAvailableReactions, mThumbnails);
            mSceneCoordinator.addReactionInDefaultLocation(mAvailableReactions.get(0));
        });
        showDialog();
    }

    /**
     * Returns the localized temporary filename with the current timestamp appended.
     */
    private String getFileName() {
        return mActivity.getString(R.string.lightweight_reactions_filename_prefix,
                String.valueOf(System.currentTimeMillis()));
    }

    /**
     * Creates the share sheet title based on a localized title and the current date formatted for
     * the user's preferred locale.
     */
    private String getShareSheetTitle() {
        Date now = new Date(System.currentTimeMillis());
        String currentDateString =
                DateFormat.getDateInstance(DateFormat.SHORT, getPreferredLocale()).format(now);
        return mActivity.getString(
                R.string.lightweight_reactions_title_for_share, currentDateString);
    }

    /**
     * Retrieves the user's preferred locale from the app's configurations.
     */
    private Locale getPreferredLocale() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                ? mActivity.getResources().getConfiguration().getLocales().get(0)
                : mActivity.getResources().getConfiguration().locale;
    }

    /**
     * Returns the time elapsed since the Lightweight Reactions dialog was opened.
     */
    private long getTimeSinceOpened() {
        return System.currentTimeMillis() - mDialogOpenedTime;
    }

    private void showErrorToast(String toastMessage) {
        if (mToast != null) {
            mToast.cancel();
        }
        mToast = Toast.makeText(mActivity, toastMessage, Toast.LENGTH_SHORT);
        mToast.show();
    }

    // LightweightReactionsCoordinator implementation.
    @Override
    public void showDialog() {
        mDialogOpenedTime = System.currentTimeMillis();
        LightweightReactionsMetrics.recordDialogOpened();
        mDialog.show(mFragmentManager, /*tag=*/null);
    }

    // BaseScreenshotCoordinator implementation.
    @Override
    protected void handleScreenshot() {
        if (mScreenshot == null) {
            showErrorToast(mActivity.getString(R.string.lightweight_reactions_error_screenshot));
            return;
        }
        mScreenshotReady = true;
        maybeFinishInitialization();
    }

    // ToolbarControlsDelegate implementation.
    @Override
    public void cancelButtonTapped() {
        LightweightReactionsMetrics.recordDialogDismissed(getTimeSinceOpened());
        LightweightReactionsMetrics.recordEditingMetrics(/*tappedDone=*/false,
                mSceneCoordinator.getNbReactionsAdded(), mSceneCoordinator.getNbTypeChange(),
                mSceneCoordinator.getNbRotateScale(), mSceneCoordinator.getNbDuplicate(),
                mSceneCoordinator.getNbDelete(), mSceneCoordinator.getNbMove());
        mDialog.dismiss();
    }

    @Override
    public void doneButtonTapped() {
        LightweightReactionsMetrics.recordEditingDone(getTimeSinceOpened());
        LightweightReactionsMetrics.recordEditingMetrics(/*tappedDone=*/true,
                mSceneCoordinator.getNbReactionsAdded(), mSceneCoordinator.getNbTypeChange(),
                mSceneCoordinator.getNbRotateScale(), mSceneCoordinator.getNbDuplicate(),
                mSceneCoordinator.getNbDelete(), mSceneCoordinator.getNbMove());
        LightweightReactionsMetrics.recordReactionsUsed(mSceneCoordinator.getReactions());

        GifGeneratorHost gifHost = new GifGeneratorHost() {
            @Override
            public void prepareFrame(Callback<Void> cb) {
                mSceneCoordinator.stepReactions(cb);
            }

            @Override
            public void drawFrame(Canvas canvas) {
                mSceneCoordinator.drawScene(canvas);
            }
        };

        mSceneCoordinator.clearSelection();
        mGenerationStartTime = System.currentTimeMillis();
        LightweightReactionsProgressDialog progressDialog =
                new LightweightReactionsProgressDialog();
        progressDialog.show(mFragmentManager, /*tag=*/null);

        mMediator.generateGif(
                gifHost, getFileName(), mSceneCoordinator, progressDialog, (imageUri) -> {
                    LightweightReactionsMetrics.recordGifGenerated(getTimeSinceOpened(),
                            imageUri != null, System.currentTimeMillis() - mGenerationStartTime);
                    final String sheetTitle = getShareSheetTitle();
                    ShareParams params =
                            new ShareParams.Builder(mWindowAndroid, sheetTitle, /*url=*/"")
                                    .setFileUris(
                                            new ArrayList<>(Collections.singletonList(imageUri)))
                                    .setFileContentType(GIF_MIME_TYPE)
                                    .setCallback(new ShareParams.TargetChosenCallback() {
                                        @Override
                                        public void onTargetChosen(ComponentName chosenComponent) {
                                            LightweightReactionsMetrics.recordGifShared(
                                                    getTimeSinceOpened(), chosenComponent);
                                        }

                                        @Override
                                        public void onCancel() {
                                            LightweightReactionsMetrics.recordGifNotShared(
                                                    getTimeSinceOpened());
                                        }
                                    })
                                    .build();

                    long shareStartTime = System.currentTimeMillis();
                    ChromeShareExtras extras =
                            new ChromeShareExtras.Builder()
                                    .setSkipPageSharingActions(true)
                                    .setContentUrl(new GURL(mShareUrl))
                                    .setDetailedContentType(
                                            DetailedContentType.LIGHTWEIGHT_REACTION)
                                    .build();

                    // Dismiss progress dialog and scene dialog before showing the share sheet.
                    progressDialog.dismiss();
                    mDialog.dismiss();
                    mChromeOptionShareCallback.showShareSheet(params, extras, shareStartTime);
                });
    }
}
