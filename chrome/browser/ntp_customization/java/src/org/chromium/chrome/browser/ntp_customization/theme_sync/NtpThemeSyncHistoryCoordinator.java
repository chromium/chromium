// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataGroup;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataImageBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** Coordinator for the NTP theme sync history. */
@NullMarked
public class NtpThemeSyncHistoryCoordinator {
    public static final int MAXIMUM_HISTORY_ITEM = 6;

    private final Context mContext;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Profile mProfile;
    private final PropertyModel mPropertyModel;
    private final NtpBackgroundDataManager mNtpBackgroundDataManager;
    private final List<NtpBackgroundDataBase> mDataShowingList;
    private final NtpThemeCollectionManager mThemeCollectionManager;
    // A list of background data including default, and color themes.
    private final List<NtpBackgroundDataBase> mDefaultOptions;
    // A list of theme collection background data ad default options.
    private final List<NtpBackgroundDataThemeCollection> mDefaultThemeCollections;
    private final Set<GURL> mLocalHistoryThemeCollectionUrlSet;

    private NtpBackgroundDataGroup @Nullable [] mNtpBackgroundDataGroups;
    private @Nullable NtpThemeSyncHistoryRecyclerViewAdaptor mRecyclerViewAdaptor;
    private @Nullable NtpBackgroundDataBase mInitiallySelectedNtpBackgroundData;
    private @Nullable ImageFetcher mImageFetcher;
    private int mLastSelectedIndex;
    private @Nullable Integer mLocalHistoryStartIndex;
    private @Nullable Integer mLocalHistoryEndIndex;

    /**
     * Creates a new instance of {@link NtpThemeSyncHistoryCoordinator}.
     *
     * @param context The activity context.
     * @param parentView The parent view that contains the history container.
     * @param bottomSheetDelegate The delegate for handling bottom sheet actions.
     * @param moreOptionsClickListener The click listener for the "More options" button.
     * @param profile The current user profile.
     */
    public NtpThemeSyncHistoryCoordinator(
            Context context,
            ViewGroup parentView,
            BottomSheetDelegate bottomSheetDelegate,
            View.OnClickListener moreOptionsClickListener,
            Profile profile) {
        this(
                context,
                parentView,
                bottomSheetDelegate,
                moreOptionsClickListener,
                new NtpThemeCollectionManager(context, profile, bitmap -> {}),
                profile);
    }

    @VisibleForTesting
    NtpThemeSyncHistoryCoordinator(
            Context context,
            ViewGroup parentView,
            BottomSheetDelegate bottomSheetDelegate,
            View.OnClickListener moreOptionsClickListener,
            NtpThemeCollectionManager themeCollectionManager,
            Profile profile) {
        mContext = context;
        mBottomSheetDelegate = bottomSheetDelegate;
        mProfile = profile;
        mThemeCollectionManager = themeCollectionManager;

        ViewGroup historyContainerView =
                parentView.findViewById(R.id.ntp_theme_sync_history_container);
        mPropertyModel = new PropertyModel(NtpThemeSyncHistoryProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel, historyContainerView, NtpThemeSyncHistoryContainerViewBinder::bind);
        setupRecyclerView(historyContainerView);

        mNtpBackgroundDataManager = new NtpBackgroundDataManager(mContext);
        mPropertyModel.set(NtpThemeSyncHistoryProperties.IS_VISIBLE, true);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mContext, LinearLayoutManager.HORIZONTAL, false);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER, layoutManager);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER,
                moreOptionsClickListener);

        mDefaultOptions = new ArrayList<>();
        mDefaultThemeCollections = new ArrayList<>();
        mLocalHistoryThemeCollectionUrlSet = new HashSet<>();
        initDefaultOptions(context);
        mDataShowingList = new ArrayList<>();
    }

    /** Initialize default options for users to choose. */
    private void initDefaultOptions(Context context) {
        mDefaultOptions.add(
                new NtpBackgroundDataColor(
                        context, PlatformType.ANDROID, NtpThemeColorId.DEFAULT, false));
        mDefaultOptions.add(
                new NtpBackgroundDataColor(
                        context, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_ORANGE, false));
        mDefaultOptions.add(
                new NtpBackgroundDataColor(
                        context, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_VIOLET, false));

        mThemeCollectionManager.getBackgroundCollections(this::onTheCollectionIdAvailable);
    }

    /**
     * Fetches the background images for the first collection once the collections are available.
     *
     * @param collections The list of available {@link BackgroundCollection}s.
     */
    private void onTheCollectionIdAvailable(@Nullable List<BackgroundCollection> collections) {
        if (collections == null || collections.isEmpty()) return;

        String firstCollectionId = collections.get(0).id;
        mThemeCollectionManager.getBackgroundImages(
                firstCollectionId, this::onThemeCollectionImageListForCollectionIdAvailable);
    }

    /**
     * Processes the list of images for a collection, fetching preview bitmaps for up to two images
     * that are not already in the local history.
     *
     * @param images The list of {@link CollectionImage}s in the collection.
     */
    private void onThemeCollectionImageListForCollectionIdAvailable(
            @Nullable List<CollectionImage> images) {
        if (images == null || images.size() == 0) return;

        int countToAdd = Math.min(2, images.size());
        int count = 0;
        int index = 0;
        while (count < countToAdd && index < images.size()) {
            CollectionImage image = images.get(index);
            index++;
            // If the URL matches a data in local history, skip it.
            if (mLocalHistoryThemeCollectionUrlSet.contains(image.imageUrl)) continue;

            count++;
            CustomBackgroundInfo info =
                    new CustomBackgroundInfo(
                            image.imageUrl,
                            image.collectionId,
                            /* isUploadedImage= */ false,
                            /* isDailyRefreshEnabled= */ false);
            if (mImageFetcher == null) {
                mImageFetcher = NtpCustomizationUtils.createImageFetcher(mProfile);
            }
            NtpCustomizationUtils.fetchThemeCollectionImage(
                    mImageFetcher,
                    image.previewImageUrl,
                    (bitmap) -> {
                        onThemeCollectionPreviewBitmapAvailable(info, bitmap);
                    });
        }
    }

    /**
     * Called when a theme collection preview bitmap is available. Creates the background data
     * object and adds it to the list of displayed items if there is space.
     *
     * @param info The {@link CustomBackgroundInfo} for the theme collection.
     * @param bitmap The preview bitmap.
     */
    private void onThemeCollectionPreviewBitmapAvailable(
            CustomBackgroundInfo info, @Nullable Bitmap bitmap) {
        if (bitmap == null) return;

        NtpBackgroundDataThemeCollection themeCollection =
                new NtpBackgroundDataThemeCollection(PlatformType.ANDROID, info, bitmap);
        mDefaultThemeCollections.add(themeCollection);
        if (mDataShowingList.size() >= MAXIMUM_HISTORY_ITEM) {
            return;
        }

        mDataShowingList.add(themeCollection);
        if (mRecyclerViewAdaptor != null) {
            mRecyclerViewAdaptor.notifyItemInserted(mDataShowingList.size() - 1);
        }
    }

    /** Setups the recycler view. */
    private void setupRecyclerView(ViewGroup parentView) {
        RecyclerView recyclerView =
                parentView.findViewById(R.id.ntp_theme_sync_history_recycler_view);
        recyclerView.setItemAnimator(null);
    }

    /**
     * Prepare data before showing the NTP theme history.
     *
     * @return The initially selected index in the showing list.
     */
    int prepareData() {
        mDataShowingList.clear();

        // The default option is placed at the first.
        mDataShowingList.add(mDefaultOptions.get(0));
        int lastSelectedIndex = RecyclerView.NO_POSITION;
        NtpBackgroundDataBase currentNtpBackgroundData =
                NtpCustomizationConfigManager.getInstance().getNtpBackgroundData();

        if (mNtpBackgroundDataGroups == null) {
            // Adds all history data to the list.
            mNtpBackgroundDataGroups =
                    mNtpBackgroundDataManager.getBackgroundDataListFromSharedPreference();
        } else {
            // Only updates the local history data.
            mNtpBackgroundDataGroups[PlatformType.ANDROID] =
                    mNtpBackgroundDataManager.getBackgroundDataGroupFromSharedPreference(
                            PlatformType.ANDROID);
        }

        int defaultOptionSize = 1;
        NtpBackgroundDataGroup localGroup = mNtpBackgroundDataGroups[PlatformType.ANDROID];
        cacheThemeCollectionUrls(localGroup);

        // Adds all previously selected theming by the users.
        assumeNonNull(localGroup);
        if (!localGroup.isEmpty()) {
            mLocalHistoryStartIndex = mDataShowingList.size();
            mDataShowingList.addAll(localGroup.getList());
            mLocalHistoryEndIndex = mDataShowingList.size() - 1;
        } else {
            mLocalHistoryStartIndex = null;
            mLocalHistoryEndIndex = null;
        }

        if (currentNtpBackgroundData == null) {
            // Sets the index to the default theme if there isn't any NTP theme set before.
            lastSelectedIndex = 0;
        } else if (!localGroup.isEmpty()) {
            int index = localGroup.indexOf(currentNtpBackgroundData);
            if (index != -1) {
                // The index is set to the item from local selected history which matches the
                // current theme.
                lastSelectedIndex = index + defaultOptionSize;
            }
        }

        // Adds sync data from remote platforms.
        for (int i = PlatformType.ANDROID + 1; i < PlatformType.MAX_COUNT; i++) {
            NtpBackgroundDataGroup remoteDataGroup = mNtpBackgroundDataGroups[i];
            if (remoteDataGroup == null || remoteDataGroup.isEmpty()) continue;

            // Finds the first remote data which isn't in the local history.
            for (NtpBackgroundDataBase data : remoteDataGroup) {
                // Checks if the current remote data exists in the local history.
                int index = localGroup.indexOf(data);
                if (index == -1) {
                    // Adds the data and stops here.
                    mDataShowingList.add(data);
                    break;
                }
            }
        }

        // Adds pre-defined color themes, starting from index 1 because index 0 (DEFAULT) is already
        // added.
        for (int i = 1; i < mDefaultOptions.size(); i++) {
            if (mDataShowingList.size() >= MAXIMUM_HISTORY_ITEM) break;

            NtpBackgroundDataBase data = mDefaultOptions.get(i);
            if (!localGroup.getList().contains(data)) {
                mDataShowingList.add(data);
            }
        }

        // Adds pre-defined theme collections images.
        for (NtpBackgroundDataThemeCollection data : mDefaultThemeCollections) {
            if (mDataShowingList.size() >= MAXIMUM_HISTORY_ITEM) break;

            if (!mLocalHistoryThemeCollectionUrlSet.contains(
                    data.getCustomBackgroundInfo().backgroundUrl)) {
                mDataShowingList.add(data);
            }
        }

        return lastSelectedIndex;
    }

    /**
     * Caches the URLs of theme collection data present in the local history to prevent showing
     * duplicates in the pre-defined options.
     *
     * @param localGroup The {@link NtpBackgroundDataGroup} containing the local history.
     */
    private void cacheThemeCollectionUrls(NtpBackgroundDataGroup localGroup) {
        mLocalHistoryThemeCollectionUrlSet.clear();

        for (NtpBackgroundDataBase data : localGroup.getList()) {
            if (data instanceof NtpBackgroundDataThemeCollection themeCollectionData) {
                mLocalHistoryThemeCollectionUrlSet.add(
                        themeCollectionData.getCustomBackgroundInfo().backgroundUrl);
            }
        }
    }

    /**
     * Prepares the history items to be shown in the customization sheet by populating the data list
     * and instantiating the adapter.
     */
    public void prepareToShow() {
        mLastSelectedIndex = prepareData();
        if (mInitiallySelectedNtpBackgroundData == null
                && mLastSelectedIndex != RecyclerView.NO_POSITION) {
            mInitiallySelectedNtpBackgroundData = mDataShowingList.get(mLastSelectedIndex);
        }

        mRecyclerViewAdaptor =
                new NtpThemeSyncHistoryRecyclerViewAdaptor(
                        mContext, mDataShowingList, this::onItemClicked, mLastSelectedIndex);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.RECYCLER_VIEW_ADAPTER, mRecyclerViewAdaptor);
        // Sets the highlighted color item if user has chosen a customized color theme.
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX, mLastSelectedIndex);
    }

    /**
     * Called when an item in the history recycler view is clicked. Applies the selected background
     * data or fetches the full-size image if it is a theme collection.
     *
     * @param backgroundData The {@link NtpBackgroundDataBase} of the clicked item.
     */
    private void onItemClicked(NtpBackgroundDataBase backgroundData, int position) {
        boolean shouldRecreate = shouldRecreateActivity(backgroundData);
        mBottomSheetDelegate.onNewColorSelected(shouldRecreate);

        if (backgroundData instanceof NtpBackgroundDataThemeCollection themeCollectionData
                && themeCollectionData.getBitmap() == null) {
            NtpCustomizationUtils.fetchThemeCollectionImage(
                    assumeNonNull(mImageFetcher),
                    themeCollectionData.getCustomBackgroundInfo().backgroundUrl,
                    (bitmap) -> {
                        onThemeCollectionImageBitmapAvailable(themeCollectionData, bitmap);
                    });
        } else {
            NtpCustomizationConfigManager.getInstance()
                    .onBackgroundDataChanged(mContext, backgroundData);

            if (isLocalHistory(position)
                    && backgroundData instanceof NtpBackgroundDataImageBase imageBaseData) {
                // When any item is clicked, we will update the local history in the
                // SharedPreference. This will lead to the existing saved bitmap being deleted. When
                // the user re-select a theme option originally from the local history, its bitmap
                // might haven deleted. Save it to the disk again.
                Bitmap bitmap = imageBaseData.getBitmap();
                if (bitmap != null) {
                    NtpCustomizationUtils.saveBackgroundImageFile(imageBaseData, bitmap);
                }
            }
        }
    }

    /**
     * Returns whether the selected theme item was originally from a local history.
     *
     * @param position The position of the item on the recyclerview.
     */
    private boolean isLocalHistory(int position) {
        if (mLocalHistoryStartIndex == null || mLocalHistoryEndIndex == null) {
            return false;
        }
        return position >= mLocalHistoryStartIndex && position <= mLocalHistoryEndIndex;
    }

    /**
     * Called when the full-size image bitmap for a theme collection is available. Updates the
     * background data with the bitmap, primary color, and file path, and applies the new theme.
     *
     * @param themeCollectionData The {@link NtpBackgroundDataThemeCollection} being updated.
     * @param bitmap The full-size image bitmap.
     */
    private void onThemeCollectionImageBitmapAvailable(
            NtpBackgroundDataThemeCollection themeCollectionData, @Nullable Bitmap bitmap) {
        if (bitmap == null || isDestroy()) return;

        CustomBackgroundInfo info = themeCollectionData.getCustomBackgroundInfo();
        String fileHashId = NtpThemeCollectionManager.getFileName(info.backgroundUrl.getPath());
        BackgroundImageInfo backgroundImageInfo =
                NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);
        @ColorInt
        Integer primaryColor =
                NtpCustomizationUtils.pickAndSavePrimaryColor(
                        assumeNonNull(themeCollectionData.getPreviewBitmap()));

        themeCollectionData.setBitmap(bitmap);
        themeCollectionData.setFileIdHash(fileHashId);
        themeCollectionData.setBackgroundImageInfo(backgroundImageInfo);
        themeCollectionData.setPrimaryColor(primaryColor);

        NtpCustomizationConfigManager.getInstance()
                .onBackgroundDataChanged(mContext, themeCollectionData);
    }

    /**
     * Determines whether the activity should be recreated to apply the new theme.
     *
     * @param backgroundData The newly selected background data.
     * @return True if the activity should be recreated, false otherwise.
     */
    private boolean shouldRecreateActivity(NtpBackgroundDataBase backgroundData) {
        return !Objects.equals(mInitiallySelectedNtpBackgroundData, backgroundData);
    }

    /** Cleans up resources and references when the coordinator is destroyed. */
    public void destroy() {
        mDataShowingList.clear();
        mInitiallySelectedNtpBackgroundData = null;
        mPropertyModel.set(NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER, null);
        mPropertyModel.set(NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER, null);
        mThemeCollectionManager.destroy();
        mRecyclerViewAdaptor = null;
        mLocalHistoryStartIndex = null;
        mLocalHistoryEndIndex = null;
    }

    private boolean isDestroy() {
        return mRecyclerViewAdaptor == null;
    }

    PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }

    List<NtpBackgroundDataBase> getDataShowingListForTesting() {
        return mDataShowingList;
    }

    @Nullable NtpThemeSyncHistoryRecyclerViewAdaptor getRecyclerViewAdaptorForTesting() {
        return mRecyclerViewAdaptor;
    }
}
