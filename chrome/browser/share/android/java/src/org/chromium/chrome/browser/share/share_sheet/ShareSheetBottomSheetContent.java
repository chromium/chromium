// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share.share_sheet;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.ui.favicon.IconType;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.List;
import java.util.Set;

/**
 * Bottom sheet content to display a 2-row custom share sheet.
 */
class ShareSheetBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private static final int SHARE_SHEET_ITEM = 0;
    private final Context mContext;
    private final LargeIconBridge mIconBridge;
    private final ShareSheetCoordinator mShareSheetCoordinator;
    private ViewGroup mContentView;
    private ShareParams mParams;
    private String mUrl;

    /**
     * Creates a ShareSheetBottomSheetContent (custom share sheet) opened from the given activity.
     *
     * @param context The context the share sheet was launched from.
     * @param iconBridge The {@link LargeIconBridge} to generate the icon in the preview.
     * @param shareSheetCoordinator The Coordinator that instantiated this BottomSheetContent.
     * @param params The {@link ShareParams} for the current share.
     */
    ShareSheetBottomSheetContent(Context context, LargeIconBridge iconBridge,
            ShareSheetCoordinator shareSheetCoordinator, ShareParams params) {
        mContext = context;
        mIconBridge = iconBridge;
        mShareSheetCoordinator = shareSheetCoordinator;
        mParams = params;
        createContentView();
    }

    private void createContentView() {
        mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.share_sheet_content, null);
    }

    /*
     * Creates a new share sheet view with two rows based on the provided PropertyModels.
     *
     * @param activity The activity the share sheet belongs to.
     * @param firstPartyModels The PropertyModels used to build the top row.
     * @param thirdPartyModels The PropertyModels used to build the bottom row.
     * @param contentTypes The {@link Set} of {@link ContentType}s to build the preview.
     * @param message The message to show on top of the share sheet.
     */
    void createRecyclerViews(List<PropertyModel> firstPartyModels,
            List<PropertyModel> thirdPartyModels, Set<Integer> contentTypes, String message) {
        // A success/failure message can be shown for features such as LinkToText.
        if (!message.isEmpty()) {
            TextView messageView = this.getContentView().findViewById(R.id.message);
            messageView.setVisibility(View.VISIBLE);
            messageView.setText(message);
            View preview = this.getContentView().findViewById(R.id.preview_header);
            preview.setVisibility(View.GONE);
        }
        // If there's no message to be shown, show a preview of the content to be shared.
        else {
            createPreview(contentTypes);
        }

        createFirstPartyRecyclerViews(firstPartyModels);

        RecyclerView thirdParty = this.getContentView().findViewById(R.id.share_sheet_other_apps);
        populateView(
                thirdPartyModels, this.getContentView().findViewById(R.id.share_sheet_other_apps));
        thirdParty.addOnScrollListener(
                new ScrollEventReporter("SharingHubAndroid.ThirdPartyAppsScrolled"));
    }

    void createFirstPartyRecyclerViews(List<PropertyModel> firstPartyModels) {
        RecyclerView firstPartyRow =
                this.getContentView().findViewById(R.id.share_sheet_chrome_apps);
        if (firstPartyModels != null && firstPartyModels.size() > 0) {
            View divider = this.getContentView().findViewById(R.id.share_sheet_divider);
            divider.setVisibility(View.VISIBLE);
            firstPartyRow.setVisibility(View.VISIBLE);
            populateView(firstPartyModels, firstPartyRow);
            firstPartyRow.addOnScrollListener(
                    new ScrollEventReporter("SharingHubAndroid.FirstPartyAppsScrolled"));
        }
    }

    private void populateView(List<PropertyModel> models, RecyclerView view) {
        ModelList modelList = new ModelList();
        for (PropertyModel model : models) {
            modelList.add(new ListItem(SHARE_SHEET_ITEM, model));
        }
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(SHARE_SHEET_ITEM, new LayoutViewBuilder(R.layout.share_sheet_item),
                ShareSheetBottomSheetContent::bindShareItem);
        view.setAdapter(adapter);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mContext, LinearLayoutManager.HORIZONTAL, false);
        view.setLayoutManager(layoutManager);
    }

    private static void bindShareItem(
            PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        if (ShareSheetItemViewProperties.ICON.equals(propertyKey)) {
            ImageView view = (ImageView) parent.findViewById(R.id.icon);
            view.setImageDrawable(model.get(ShareSheetItemViewProperties.ICON));
        } else if (ShareSheetItemViewProperties.LABEL.equals(propertyKey)) {
            TextView view = (TextView) parent.findViewById(R.id.text);
            view.setText(model.get(ShareSheetItemViewProperties.LABEL));
        } else if (ShareSheetItemViewProperties.CLICK_LISTENER.equals(propertyKey)) {
            parent.setOnClickListener(model.get(ShareSheetItemViewProperties.CLICK_LISTENER));
        }
    }

    private void createPreview(Set<Integer> contentTypes) {
        // Default preview is to show title + url.
        String title = mParams.getTitle();
        String subtitle = mParams.getUrl();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            fetchFavicon(mParams.getUrl());
            setTitleStyle(R.style.TextAppearance_TextMediumThick_Primary);
            setTextForPreview(title, subtitle);
            return;
        }

        if (contentTypes.contains(ContentType.IMAGE)) {
            setImageForPreviewFromUri(mParams.getFileUris().get(0));
            if (TextUtils.isEmpty(subtitle)) {
                subtitle = mContext.getResources().getString(
                        R.string.sharing_hub_image_preview_subtitle);
            }
        } else if (contentTypes.contains(ContentType.OTHER_FILE_TYPE)) {
            // TODO(1120093): Set file icon.
        } else if (contentTypes.size() == 1
                && (contentTypes.contains(ContentType.HIGHLIGHTED_TEXT)
                        || contentTypes.contains(ContentType.TEXT))) {
            // TODO(1120093): Set text monogram icon.
            title = "";
            subtitle = mParams.getText();
            setSubtitleMaxLines(2);
        } else {
            fetchFavicon(mParams.getUrl());
        }

        if (contentTypes.contains(ContentType.TEXT)
                && contentTypes.contains(ContentType.LINK_PAGE_NOT_VISIBLE)) {
            title = mParams.getText();
            setTitleStyle(R.style.TextAppearance_TextMedium_Primary);
        } else {
            setTitleStyle(R.style.TextAppearance_TextMediumThick_Primary);
        }

        setTextForPreview(title, subtitle);
    }

    private void setImageForPreviewFromUri(Uri imageUri) {
        try {
            setImagePreview(
                    ApiCompatibilityUtils.getBitmapByUri(mContext.getContentResolver(), imageUri));
        } catch (IOException e) {
            // If no image preview available, don't show a preview.
        }
    }

    private void setTitleStyle(int resId) {
        TextView titleView = this.getContentView().findViewById(R.id.title_preview);
        ApiCompatibilityUtils.setTextAppearance(titleView, resId);
    }

    private void setTextForPreview(String title, String subtitle) {
        TextView titleView = this.getContentView().findViewById(R.id.title_preview);
        titleView.setText(title);
        TextView subtitleView = this.getContentView().findViewById(R.id.subtitle_preview);
        subtitleView.setText(subtitle);

        // If there is no title, have subtitleView take up the whole area.
        if (TextUtils.isEmpty(title)) {
            titleView.setVisibility(View.GONE);
        }
    }

    private void setSubtitleMaxLines(int maxLines) {
        TextView subtitleView = this.getContentView().findViewById(R.id.subtitle_preview);
        subtitleView.setMaxLines(maxLines);
    }

    private void setImagePreview(Bitmap icon) {
        ImageView imageView = this.getContentView().findViewById(R.id.image_preview);
        imageView.setImageBitmap(icon);
    }

    /**
     * Fetches the favicon for the given url.
     **/
    private void fetchFavicon(String url) {
        if (!url.isEmpty()) {
            mUrl = url;
            mIconBridge.getLargeIconForUrl(new GURL(url),
                    mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size),
                    this::onFaviconAvailable);
        }
    }

    /**
     * Passed as the callback to {@link LargeIconBridge#getLargeIconForStringUrl}
     * by showShareSheetWithMessage.
     */
    private void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(mUrl);
            // generateIconForUrl might return null if the URL is empty or the domain cannot be
            // resolved. See https://crbug.com/987101
            // TODO(1120093): Handle the case where generating an icon fails.
            if (icon == null) {
                return;
            }
        }

        int size = mContext.getResources().getDimensionPixelSize(
                R.dimen.sharing_hub_preview_monogram_size);

        setImagePreview(Bitmap.createScaledBitmap(icon, size, size, true));
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        Resources resources = mContext.getResources();
        int iconSize = resources.getDimensionPixelSize(R.dimen.sharing_hub_preview_monogram_size);
        int cornerRadius = iconSize / 2;
        int textSize =
                resources.getDimensionPixelSize(R.dimen.sharing_hub_preview_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }

    /**
     * One-shot reporter that records the first time the user scrolls a {@link RecyclerView}.
     */
    private static class ScrollEventReporter extends RecyclerView.OnScrollListener {
        private boolean mFired;
        private String mActionName;

        public ScrollEventReporter(String actionName) {
            mActionName = actionName;
        }

        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            if (mFired) return;
            if (newState != RecyclerView.SCROLL_STATE_DRAGGING) return;

            RecordUserAction.record(mActionName);
            mFired = true;
        }
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    protected View getFirstPartyView() {
        return mContentView.findViewById(R.id.share_sheet_chrome_apps);
    }

    protected View getThirdPartyView() {
        return mContentView.findViewById(R.id.share_sheet_other_apps);
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mShareSheetCoordinator.destroy();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Return false to ensure that the entire bottom sheet is shown.
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // Return WRAP_CONTENT to have the bottom sheet only open as far as it needs to display the
        // list of devices and nothing beyond that.
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {}
}
