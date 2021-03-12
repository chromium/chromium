// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share.share_sheet;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
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
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.Toast;
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
    private @LinkGeneration int mLinkGenerationState;
    private Toast mToast;

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
        mLinkGenerationState =
                mParams.getLinkToTextSuccessful() != null && mParams.getLinkToTextSuccessful()
                ? LinkGeneration.LINK
                : LinkGeneration.FAILURE;
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
        populateView(thirdPartyModels,
                this.getContentView().findViewById(R.id.share_sheet_other_apps),
                /*firstParty=*/false);
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
            populateView(firstPartyModels, firstPartyRow, /*firstParty=*/true);
            firstPartyRow.addOnScrollListener(
                    new ScrollEventReporter("SharingHubAndroid.FirstPartyAppsScrolled"));
        }
    }

    void updateShareParams(ShareParams params) {
        mParams = params;
    }

    @LinkGeneration
    int getLinkGenerationState() {
        return mLinkGenerationState;
    }

    private void populateView(List<PropertyModel> models, RecyclerView view, boolean firstParty) {
        ModelList modelList = new ModelList();
        for (PropertyModel model : models) {
            modelList.add(new ListItem(SHARE_SHEET_ITEM, model));
        }
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(SHARE_SHEET_ITEM, new LayoutViewBuilder(R.layout.share_sheet_item),
                (firstParty ? ShareSheetBottomSheetContent::bindShareItem
                            : ShareSheetBottomSheetContent::bind3PShareItem));
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
            View layout = (View) parent.findViewById(R.id.layout);
            layout.setOnClickListener(model.get(ShareSheetItemViewProperties.CLICK_LISTENER));
        } else if (ShareSheetItemViewProperties.SHOW_NEW_BADGE.equals(propertyKey)) {
            TextView newBadge = (TextView) parent.findViewById(R.id.display_new);
            newBadge.setVisibility(model.get(ShareSheetItemViewProperties.SHOW_NEW_BADGE)
                            ? View.VISIBLE
                            : View.GONE);
        }
    }

    private static void bind3PShareItem(
            PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        bindShareItem(model, parent, propertyKey);
        if (ShareSheetItemViewProperties.ICON.equals(propertyKey)) {
            ImageView view = (ImageView) parent.findViewById(R.id.icon);
            View layout = (View) parent.findViewById(R.id.layout);

            final int iconSize =
                    ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                            R.dimen.sharing_hub_3p_icon_size);
            final int paddingTop =
                    ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                            R.dimen.sharing_hub_3p_icon_padding_top);
            ViewGroup.LayoutParams params = view.getLayoutParams();
            params.height = iconSize;
            params.width = iconSize;
            view.requestLayout();
            layout.setPadding(0, paddingTop, 0, 0);
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

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                && contentTypes.contains(ContentType.HIGHLIGHTED_TEXT)) {
            setLinkImageViewForPreview();
        }

        if ((contentTypes.contains(ContentType.TEXT)
                    || contentTypes.contains(ContentType.HIGHLIGHTED_TEXT))
                && contentTypes.contains(ContentType.LINK_PAGE_NOT_VISIBLE)) {
            title = mParams.getText();
            setTitleStyle(R.style.TextAppearance_TextMedium_Primary);
            setSubtitleMaxLines(1);
        } else if (!TextUtils.isEmpty(title)) {
            // Set title style if title is non empty.
            setTitleStyle(R.style.TextAppearance_TextMediumThick_Primary);
        }

        setTextForPreview(title, subtitle);
    }

    private void setImageForPreviewFromUri(Uri imageUri) {
        try {
            Bitmap bitmap =
                    ApiCompatibilityUtils.getBitmapByUri(mContext.getContentResolver(), imageUri);
            // We don't want to use hardware bitmaps in case of software rendering. See
            // https://crbug.com/1172883.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && isHardwareBitmap(bitmap)) {
                bitmap = bitmap.copy(Bitmap.Config.ARGB_8888, /*mutable=*/false);
            }
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

    @TargetApi(Build.VERSION_CODES.O)
    private boolean isHardwareBitmap(Bitmap bitmap) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return bitmap.getConfig() == Bitmap.Config.HARDWARE;
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
            return;
        }

        titleView.setVisibility(View.VISIBLE);
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

    public void updateLinkGenerationState() {
        if (mLinkGenerationState == LinkGeneration.FAILURE) return;
        if (mLinkGenerationState == LinkGeneration.LINK) {
            mLinkGenerationState = LinkGeneration.TEXT;
        } else {
            mLinkGenerationState = LinkGeneration.LINK;
        }
    }

    private void setLinkImageViewForPreview() {
        int drawable = 0;
        int contentDescription = 0;
        int skillColor = 0;

        switch (mLinkGenerationState) {
            case LinkGeneration.FAILURE:
                drawable = R.drawable.link_off;
                contentDescription = R.string.link_to_text_failure_toast_message_v2;
                skillColor = R.color.default_icon_color;
                break;
            case LinkGeneration.LINK:
                drawable = R.drawable.link;
                contentDescription = R.string.link_to_text_success_link_toast_message;
                skillColor = R.color.default_icon_color_blue;
                break;
            case LinkGeneration.TEXT:
                drawable = R.drawable.link_off;
                contentDescription = R.string.link_to_text_success_text_toast_message;
                skillColor = R.color.default_icon_color;
                break;
        }

        ImageView linkImageView = this.getContentView().findViewById(R.id.image_preview_link);
        linkImageView.setColorFilter(ContextCompat.getColor(mContext, skillColor));
        linkImageView.setVisibility(View.VISIBLE);
        linkImageView.setImageDrawable(AppCompatResources.getDrawable(mContext, drawable));
        linkImageView.setContentDescription(mContext.getResources().getString(contentDescription));
        centerIcon(linkImageView);

        linkImageView.setOnClickListener(v -> {
            updateLinkGenerationState();
            switch (mLinkGenerationState) {
                case LinkGeneration.FAILURE:
                    showToast(R.string.link_to_text_failure_toast_message_v2);
                    linkImageView.setContentDescription(mContext.getResources().getString(
                            R.string.link_to_text_failure_toast_message_v2));
                    RecordUserAction.record("SharingHubAndroid.LinkGeneration.Failure");
                    break;
                case LinkGeneration.LINK:
                    showToast(R.string.link_to_text_success_link_toast_message);
                    linkImageView.setImageDrawable(
                            AppCompatResources.getDrawable(mContext, R.drawable.link));
                    linkImageView.setContentDescription(mContext.getResources().getString(
                            R.string.link_to_text_success_link_toast_message));
                    mShareSheetCoordinator.updateShareSheetForLinkToText(LinkGeneration.LINK);
                    RecordUserAction.record("SharingHubAndroid.LinkGeneration.Link");
                    break;
                case LinkGeneration.TEXT:
                    showToast(R.string.link_to_text_success_text_toast_message);
                    linkImageView.setImageDrawable(
                            AppCompatResources.getDrawable(mContext, R.drawable.link_off));
                    linkImageView.setContentDescription(mContext.getResources().getString(
                            R.string.link_to_text_success_text_toast_message));
                    mShareSheetCoordinator.updateShareSheetForLinkToText(LinkGeneration.TEXT);
                    RecordUserAction.record("SharingHubAndroid.LinkGeneration.Text");
                    break;
            }
        });
    }

    private void showToast(int resource) {
        if (mToast != null) {
            mToast.cancel();
        }
        String toastMessage = mContext.getResources().getString(resource);
        mToast = Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT);
        mToast.setGravity(mToast.getGravity(), mToast.getXOffset(),
                mContext.getResources().getDimensionPixelSize(R.dimen.y_offset_full_sharesheet));
        mToast.show();
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
        if (mToast != null) {
            mToast.cancel();
        }
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
