// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share.share_sheet;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
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
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.url_formatter.UrlFormatter;
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
    private ScrollView mContentScrollableView;

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
        mContentScrollableView = mContentView.findViewById(R.id.share_sheet_scrollview);
    }

    /*
     * Creates a new share sheet view with two rows based on the provided PropertyModels.
     *
     * @param activity The activity the share sheet belongs to.
     * @param firstPartyModels The PropertyModels used to build the top row.
     * @param thirdPartyModels The PropertyModels used to build the bottom row.
     * @param contentTypes The {@link Set} of {@link ContentType}s to build the preview.
     * @param fileContentType The MIME type of the file(s) being shared.
     */
    void createRecyclerViews(List<PropertyModel> firstPartyModels,
            List<PropertyModel> thirdPartyModels, Set<Integer> contentTypes,
            String fileContentType) {
        createPreview(contentTypes, fileContentType);
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

    private void createPreview(Set<Integer> contentTypes, String fileContentType) {
        // Default preview is to show title + url.
        String title = mParams.getTitle();
        String subtitle =
                UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(mParams.getUrl());
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            fetchFavicon(mParams.getUrl());
            setTitleStyle(R.style.TextAppearance_TextMediumThick_Primary);
            setTextForPreview(title, subtitle);
            return;
        }

        if (contentTypes.contains(ContentType.IMAGE)) {
            setImageForPreviewFromUri(mParams.getFileUris().get(0));
            if (TextUtils.isEmpty(subtitle)) {
                subtitle = getFileType(fileContentType);
            }
        } else if (contentTypes.contains(ContentType.OTHER_FILE_TYPE)) {
            setDefaultIconForPreview(
                    AppCompatResources.getDrawable(mContext, R.drawable.generic_file));
            if (TextUtils.isEmpty(subtitle)) {
                subtitle = getFileType(fileContentType);
            }
        } else if (contentTypes.size() == 1
                && (contentTypes.contains(ContentType.HIGHLIGHTED_TEXT)
                        || contentTypes.contains(ContentType.TEXT))) {
            setDefaultIconForPreview(
                    AppCompatResources.getDrawable(mContext, R.drawable.text_icon));
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
            Bitmap bitmap =
                    ApiCompatibilityUtils.getBitmapByUri(mContext.getContentResolver(), imageUri);
            RoundedCornerImageView imageView =
                    this.getContentView().findViewById(R.id.image_preview);
            imageView.setImageBitmap(bitmap);
            imageView.setRoundedFillColor(ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.default_icon_color));
            imageView.setScaleType(ScaleType.FIT_CENTER);
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

    private void setDefaultIconForPreview(Drawable drawable) {
        ImageView imageView = this.getContentView().findViewById(R.id.image_preview);
        imageView.setImageDrawable(drawable);
        centerIcon(imageView);
    }

    private void centerIcon(ImageView imageView) {
        imageView.setScaleType(ScaleType.FIT_XY);
        int padding = mContext.getResources().getDimensionPixelSize(
                R.dimen.sharing_hub_preview_icon_padding);
        imageView.setPadding(padding, padding, padding, padding);
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
        // If we didn't get a favicon, use the generic favicon instead.
        if (icon == null) {
            setDefaultIconForPreview(
                    AppCompatResources.getDrawable(mContext, R.drawable.generic_favicon));
            RecordUserAction.record("SharingHubAndroid.GenericFaviconShown");
        } else {
            int size = mContext.getResources().getDimensionPixelSize(
                    R.dimen.sharing_hub_preview_inner_icon_size);
            Bitmap scaledIcon = Bitmap.createScaledBitmap(icon, size, size, true);
            ImageView imageView = this.getContentView().findViewById(R.id.image_preview);
            imageView.setImageBitmap(scaledIcon);
            centerIcon(imageView);
            RecordUserAction.record("SharingHubAndroid.LinkFaviconShown");
        }
    }

    private String getFileType(String mimeType) {
        if (!mimeType.contains("/")) {
            return "";
        }
        String supertype = mimeType.split("/", 2)[0];
        // Accepted MIME types are drawn from
        // //chrome/browser/webshare/share_service_impl.cc
        switch (supertype) {
            case "audio":
                return mContext.getResources().getString(
                        R.string.sharing_hub_audio_preview_subtitle);
            case "image":
                return mContext.getResources().getString(
                        R.string.sharing_hub_image_preview_subtitle);
            case "text":
                return mContext.getResources().getString(
                        R.string.sharing_hub_text_preview_subtitle);
            case "video":
                return mContext.getResources().getString(
                        R.string.sharing_hub_video_preview_subtitle);
            default:
                return "";
        }
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
        if (mContentScrollableView != null) {
            return mContentScrollableView.getScrollY();
        }

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
