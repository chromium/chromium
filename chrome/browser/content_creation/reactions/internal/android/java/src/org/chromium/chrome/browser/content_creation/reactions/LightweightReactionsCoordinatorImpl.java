// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.content_creation.reactions.scene.SceneCoordinator;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarControlsDelegate;
import org.chromium.chrome.browser.content_creation.reactions.toolbar.ToolbarCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.BaseScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.components.content_creation.reactions.ReactionService;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;

import java.util.List;

/**
 * Responsible for reactions main UI and its subcomponents.
 */
public class LightweightReactionsCoordinatorImpl extends BaseScreenshotCoordinator
        implements LightweightReactionsCoordinator, ToolbarControlsDelegate {
    private final ReactionService mReactionService;
    private final LightweightReactionsMediator mMediator;
    private final LightweightReactionsDialog mDialog;
    private final SceneCoordinator mSceneCoordinator;

    private List<ReactionMetadata> mAvailableReactions;
    private Bitmap[] mThumbnails;
    private ToolbarCoordinator mToolbarCoordinator;

    private boolean mDialogViewCreated;
    private boolean mAssetsFetched;

    /**
     * Constructs a new LightweightReactionsCoordinatorImpl which initializes and displays the
     * Lightweight Reactions scene.
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param reactionService The {@link ReactionService} to use for Reaction operations.
     */
    public LightweightReactionsCoordinatorImpl(Activity activity, Tab tab, String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController, ReactionService reactionService) {
        super(activity, tab, shareUrl, chromeOptionShareCallback, sheetController);
        mDialogViewCreated = false;
        mAssetsFetched = false;
        mReactionService = reactionService;
        mDialog = new LightweightReactionsDialog();
        mSceneCoordinator = new SceneCoordinator(activity);

        Profile profile = Profile.fromWebContents(tab.getWebContents());
        ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.DISK_CACHE_ONLY, profile.getProfileKey());
        mMediator = new LightweightReactionsMediator(imageFetcher);
        mReactionService.getReactions((reactions) -> {
            mAvailableReactions = reactions;
            mMediator.fetchAssetsAndGetThumbnails(reactions, this::onAssetsFetched);
        });
    }

    /**
     * Creates the toolbar coordinator after the root dialog view is ready, then attempts to finish
     * the initialization of the feature.
     * @param view The root {@link View} of the dialog.
     */
    private void onViewCreated(View view) {
        mDialogViewCreated = true;
        mToolbarCoordinator = new ToolbarCoordinator(view, this, mSceneCoordinator);
        maybeFinishInitialization();
    }

    /**
     * Stores the thumbnails of the reactions, then attempts to finish the initialization of the
     * feature.
     * @param thumbnails The list of thumbnails. The order must be the same as
     *                   {@code mAvailableReactions}.
     */
    private void onAssetsFetched(Bitmap[] thumbnails) {
        assert thumbnails != null;
        assert thumbnails.length == mAvailableReactions.size();

        mAssetsFetched = true;
        mThumbnails = thumbnails;
        maybeFinishInitialization();
    }

    /**
     * Performs the remaining initialization for the feature, namely creating the toolbar carousel
     * for the reactions, hooking up the remaining event handlers, and dismissing the loading view.
     *
     * <p><b>Note:</b> The dialog view must be ready and the assets must have been fetched. If
     * either is missing, this is a no-op.
     */
    private void maybeFinishInitialization() {
        if (!mDialogViewCreated || !mAssetsFetched) {
            // Wait until both operations have completed.
            return;
        }
        mToolbarCoordinator.initReactions(mAvailableReactions, mThumbnails);
    }

    // LightweightReactionsCoordinator implementation.
    @Override
    public void showDialog() {
        FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
        mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
    }

    // BaseScreenshotCoordinator implementation.
    @Override
    protected void handleScreenshot() {
        mDialog.init(mScreenshot, mSceneCoordinator, this::onViewCreated);
        showDialog();
    }

    // ToolbarControlsDelegate implementation.
    @Override
    public void cancelButtonTapped() {
        mDialog.dismiss();
    }

    @Override
    public void doneButtonTapped() {
        // For now, simply dismiss the dialog.
        mDialog.dismiss();
    }
}
