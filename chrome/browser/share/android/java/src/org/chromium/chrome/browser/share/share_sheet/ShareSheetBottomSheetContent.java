// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
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
import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;

/** Bottom sheet content to display a 2-row custom share sheet. */
class ShareSheetBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private static final int SHARE_SHEET_ITEM = 0;

    private final Activity mActivity;
    private final Profile mProfile;
    private final LargeIconBridge mIconBridge;
    private final ShareSheetCoordinator mShareSheetCoordinator;
    private final Tracker mFeatureEngagementTracker;
    private ViewGroup mContentView;
    private ShareParams mParams;
    private ScrollView mContentScrollableView;
    private @LinkGeneration int mLinkGenerationState;
    private @LinkToggleState Integer mLinkToggleState;
    private Toast mToast;

    /**
     * Creates a ShareSheetBottomSheetContent (custom share sheet) opened from the given activity.
     *
     * @param activity The containing {@link Activity}.
     * @param profile The active {@link Profile}.
     * @param iconBridge The {@link LargeIconBridge} to generate the icon in the preview.
     * @param shareSheetCoordinator The Coordinator that instantiated this BottomSheetContent.
     * @param params The {@link ShareParams} for the current share.
     * @param featureEngagementTracker The {@link Tracker} for tracking feature engagement.
     */
    ShareSheetBottomSheetContent(
            Activity activity,
            Profile profile,
            LargeIconBridge iconBridge,
            ShareSheetCoordinator shareSheetCoordinator,
            ShareParams params,
            Tracker featureEngagementTracker) {
        mActivity = activity;
        mProfile = profile;
        mIconBridge = iconBridge;
        mShareSheetCoordinator = shareSheetCoordinator;
        mParams = params;
        mFeatureEngagementTracker = featureEngagementTracker;

        // Set |mLinkGenerationState| to invalid value of |MAX| if |getLinkToTextSuccessful|
        // is not set in order to distinguish it from failure state. |getLinkToTextSuccessful| will
        // be set only for link to text.
        if (mParams.getLinkToTextSuccessful() == null) {
            mLinkGenerationState = LinkGeneration.MAX;
        } else {
            if (mParams.getLinkToTextSuccessful()) {
                mLinkGenerationState = LinkGeneration.LINK;
                mLinkToggleState = LinkToggleState.LINK;
            } else {
                mLinkGenerationState = LinkGeneration.FAILURE;
                mLinkToggleState = LinkToggleState.NO_LINK;
            }
        }
        createContentView();
    }

    private void createContentView() {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(mActivity).inflate(R.layout.share_sheet_content, null);
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
     * @param detailedContentType The {@link DetailedContentType} of the content being shared.
     * @param shareSheetLinkToggleCoordinator The {@link ShareSheetLinkToggleCoordinator} for
     *         whether to show the toggle and the default enabled status.
     */
    void createRecyclerViews(
            List<PropertyModel> firstPartyModels,
            List<PropertyModel> thirdPartyModels,
            Set<Integer> contentTypes,
            String fileContentType,
            @DetailedContentType int detailedContentType,
            ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator) {
        createPreview(
                contentTypes,
                fileContentType,
                detailedContentType,
                shareSheetLinkToggleCoordinator);
        createFirstPartyRecyclerViews(firstPartyModels);

        RecyclerView thirdParty = this.getContentView().findViewById(R.id.share_sheet_other_apps);
        // Disable third party share options for automotive.
        if (BuildInfo.getInstance().isAutomotive) {
            thirdParty.setVisibility(View.GONE);
            this.getContentView().findViewById(R.id.share_sheet_divider).setVisibility(View.GONE);
            return;
        }
        populateView(
                thirdPartyModels,
                this.getContentView().findViewById(R.id.share_sheet_other_apps),
                /* firstParty= */ false);
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
            populateView(firstPartyModels, firstPartyRow, /* firstParty= */ true);
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
        adapter.registerType(
                SHARE_SHEET_ITEM,
                new LayoutViewBuilder(R.layout.share_sheet_item),
                (firstParty
                        ? ShareSheetBottomSheetContent::bindShareItem
                        : ShareSheetBottomSheetContent::bind3PShareItem));
        view.setAdapter(adapter);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mActivity, LinearLayoutManager.HORIZONTAL, false);
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
        } else if (ShareSheetItemViewProperties.CONTENT_DESCRIPTION.equals(propertyKey)) {
            TextView view = (TextView) parent.findViewById(R.id.text);
            view.setContentDescription(model.get(ShareSheetItemViewProperties.CONTENT_DESCRIPTION));
        } else if (ShareSheetItemViewProperties.CLICK_LISTENER.equals(propertyKey)) {
            View layout = (View) parent.findViewById(R.id.layout);
            layout.setOnClickListener(model.get(ShareSheetItemViewProperties.CLICK_LISTENER));
        } else if (ShareSheetItemViewProperties.SHOW_NEW_BADGE.equals(propertyKey)) {
            TextView newBadge = (TextView) parent.findViewById(R.id.display_new);
            newBadge.setVisibility(
                    model.get(ShareSheetItemViewProperties.SHOW_NEW_BADGE)
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
                    ContextUtils.getApplicationContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.sharing_hub_3p_icon_size);
            final int paddingTop =
                    ContextUtils.getApplicationContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.sharing_hub_3p_icon_padding_top);
            ViewGroup.LayoutParams params = view.getLayoutParams();
            params.height = iconSize;
            params.width = iconSize;
            ViewUtils.requestLayout(view, "ShareSheetBottomSheetContent.bind3PShareItem");
            layout.setPadding(0, paddingTop, 0, 0);
        }
    }

    private void createPreview(
            Set<Integer> contentTypes,
            String fileContentType,
            @DetailedContentType int detailedContentType,
            ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator) {
        // Default preview is to show title + url.
        String title = mParams.getTitle();
        String subtitle =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mParams.getUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);

        if (contentTypes.contains(ContentType.IMAGE)
                || contentTypes.contains(ContentType.IMAGE_AND_LINK)) {
            assert mParams.getImageUriToShare() != null;
            setImageForPreviewFromUri(mParams.getImageUriToShare());
            if (TextUtils.isEmpty(subtitle)) {
                subtitle = getFileType(fileContentType);
            }
        } else if (contentTypes.contains(ContentType.OTHER_FILE_TYPE)) {
            setDefaultIconForPreview(
                    AppCompatResources.getDrawable(mActivity, R.drawable.generic_file));
            if (TextUtils.isEmpty(subtitle)) {
                subtitle = getFileType(fileContentType);
            }
        } else if (contentTypes.size() == 1
                && (contentTypes.contains(ContentType.HIGHLIGHTED_TEXT)
                        || contentTypes.contains(ContentType.TEXT))) {
            setDefaultIconForPreview(
                    AppCompatResources.getDrawable(mActivity, R.drawable.text_icon));
            title = "";
            subtitle = mParams.getText();
            setSubtitleMaxLines(2);
        } else {
            fetchFavicon(mParams.getUrl());
        }

        if (shareSheetLinkToggleCoordinator.shouldShowToggle()) {
            if (mLinkToggleState == null) {
                setDefaultToggleStatus(
                        shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());
            }
            setLinkToggleForPreview(detailedContentType);
        }

        if ((contentTypes.contains(ContentType.TEXT)
                        || contentTypes.contains(ContentType.HIGHLIGHTED_TEXT))
                && contentTypes.contains(ContentType.LINK_PAGE_NOT_VISIBLE)) {
            title = mParams.getPreviewText() != null ? mParams.getPreviewText() : mParams.getText();
            setTitleStyle(R.style.TextAppearance_TextMedium_Primary);
            setSubtitleMaxLines(1);
        } else if (!TextUtils.isEmpty(title)) {
            // Set title style if title is non empty.
            setTitleStyle(R.style.TextAppearance_TextMediumThick_Primary);
        }

        setTextForPreview(title, subtitle);
    }

    private void setImageForPreviewFromUri(Uri imageUri) {
        ShareImageFileUtils.getBitmapFromUriAsync(
                mActivity, imageUri, this::setImageForPreviewFromBitmap);
    }

    private void setImageForPreviewFromBitmap(Bitmap bitmap) {
        // If no image preview available, don't show a preview.
        if (bitmap == null) return;

        RoundedCornerImageView imageView = this.getContentView().findViewById(R.id.image_preview);
        imageView.setImageBitmap(bitmap);
        imageView.setRoundedFillColor(SemanticColorUtils.getDefaultIconColor(mActivity));
        imageView.setScaleType(ScaleType.FIT_CENTER);
    }

    private void setTitleStyle(int resId) {
        TextView titleView = this.getContentView().findViewById(R.id.title_preview);
        titleView.setTextAppearance(resId);
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

    private void updateLinkToggleState(@DetailedContentType int detailedContentType) {
        int toastMessage;
        if (mLinkToggleState == LinkToggleState.NO_LINK) {
            mLinkToggleState = LinkToggleState.LINK;
            toastMessage = R.string.link_toggle_include_link;
        } else {
            mLinkToggleState = LinkToggleState.NO_LINK;
            toastMessage = getExcludeLinkToast(detailedContentType);
        }
        showToast(toastMessage);
        LinkToggleMetricsDetails linkToggleMetricsDetails =
                new LinkToggleMetricsDetails(mLinkToggleState, detailedContentType);
        ShareSheetLinkToggleMetricsHelper.recordLinkToggleToggledMetric(linkToggleMetricsDetails);
        mShareSheetCoordinator.updateShareSheetForLinkToggle(
                linkToggleMetricsDetails, mLinkGenerationState);
    }

    private void updateLinkGenerationState() {
        int toastMessage = 0;
        String userAction = "";
        if (mLinkGenerationState == LinkGeneration.LINK) {
            mLinkGenerationState = LinkGeneration.TEXT;
            mLinkToggleState = LinkToggleState.NO_LINK;
            toastMessage = R.string.link_to_text_success_text_toast_message;
            userAction = "SharingHubAndroid.LinkGeneration.Text";
        } else if (mLinkGenerationState == LinkGeneration.TEXT) {
            mLinkGenerationState = LinkGeneration.LINK;
            mLinkToggleState = LinkToggleState.LINK;
            toastMessage = R.string.link_to_text_success_link_toast_message;
            userAction = "SharingHubAndroid.LinkGeneration.Link";
        } else if (mLinkGenerationState == LinkGeneration.FAILURE) {
            mLinkToggleState = LinkToggleState.NO_LINK;
            toastMessage = R.string.link_to_text_failure_toast_message_v2;
            userAction = "SharingHubAndroid.LinkGeneration.Failure";
        }

        showToast(toastMessage);
        RecordUserAction.record(userAction);
        mShareSheetCoordinator.updateShareSheetForLinkToggle(
                new LinkToggleMetricsDetails(
                        mLinkToggleState, DetailedContentType.HIGHLIGHTED_TEXT),
                mLinkGenerationState);
    }

    private void setDefaultToggleStatus(boolean enableToggleByDefault) {
        mLinkToggleState = enableToggleByDefault ? LinkToggleState.LINK : LinkToggleState.NO_LINK;
    }

    private int getExcludeLinkToast(@DetailedContentType int detailedContentType) {
        switch (detailedContentType) {
            case DetailedContentType.IMAGE:
                return R.string.link_toggle_share_image_only;
            case DetailedContentType.GIF:
                return R.string.link_toggle_share_gif_only;
            case DetailedContentType.SCREENSHOT:
                return R.string.link_toggle_share_screenshot_only;
            case DetailedContentType.HIGHLIGHTED_TEXT:
            case DetailedContentType.NOT_SPECIFIED:
                return R.string.link_toggle_share_content_only;
            default:
                return 0;
        }
    }

    private void setLinkToggleForPreview(@DetailedContentType int detailedContentType) {
        int drawable;
        int contentDescription;
        @ColorRes int skillColor;

        if (mLinkToggleState == LinkToggleState.LINK) {
            drawable = R.drawable.link;
            skillColor = R.color.default_icon_color_accent1_tint_list;
            contentDescription = R.string.link_toggle_include_link;
        } else {
            drawable = R.drawable.link_off;
            skillColor = R.color.default_icon_color_tint_list;
            contentDescription = getExcludeLinkToast(detailedContentType);
        }

        if (detailedContentType == DetailedContentType.HIGHLIGHTED_TEXT) {
            if (mLinkGenerationState == LinkGeneration.LINK) {
                contentDescription = R.string.link_to_text_success_link_toast_message;
            } else if (mLinkGenerationState == LinkGeneration.TEXT) {
                contentDescription = R.string.link_to_text_success_text_toast_message;
            } else if (mLinkGenerationState == LinkGeneration.FAILURE) {
                contentDescription = R.string.link_to_text_failure_toast_message_v2;
            }
        }

        ImageView linkToggleView = getContentView().findViewById(R.id.link_toggle_view);
        linkToggleView.setColorFilter(
                AppCompatResources.getColorStateList(mActivity, skillColor).getDefaultColor());
        linkToggleView.setVisibility(View.VISIBLE);
        linkToggleView.setImageDrawable(AppCompatResources.getDrawable(mActivity, drawable));
        // This is necessary in order to prevent voice over announcing the content description
        // change. See https://crbug.com/1192666.
        linkToggleView.setContentDescription(null);
        linkToggleView.setContentDescription(
                mActivity.getResources().getString(contentDescription));
        centerIcon(linkToggleView);
        if (mLinkToggleState == LinkToggleState.NO_LINK) {
            maybeShowToggleIph();
        }

        linkToggleView.setOnClickListener(
                v -> {
                    mFeatureEngagementTracker.notifyEvent(
                            EventConstants.SHARING_HUB_LINK_TOGGLE_CLICKED);
                    if (detailedContentType == DetailedContentType.HIGHLIGHTED_TEXT) {
                        updateLinkGenerationState();
                    } else {
                        updateLinkToggleState(detailedContentType);
                    }
                });
    }

    /**
     * Shows the IPH for the toggle if the link is turned off by default and if it meets the feature
     * engagement tracker requirements.
     */
    void maybeShowToggleIph() {
        View anchorView = getContentView().findViewById(R.id.link_toggle_view);
        int yInsetPx = mActivity.getResources().getDimensionPixelOffset(R.dimen.toggle_iph_y_inset);
        Rect insetRect = new Rect(0, -yInsetPx, 0, -yInsetPx);

        UserEducationHelper userEducationHelper =
                new UserEducationHelper(mActivity, mProfile, new Handler(Looper.getMainLooper()));
        userEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.IPH_SHARING_HUB_LINK_TOGGLE_FEATURE,
                                R.string.link_toggle_iph,
                                R.string.link_toggle_iph)
                        .setAnchorView(anchorView)
                        .setHighlightParams(
                                new ViewHighlighter.HighlightParams(
                                        ViewHighlighter.HighlightShape.CIRCLE))
                        .setInsetRect(insetRect)
                        .setPreferredVerticalOrientation(
                                AnchoredPopupWindow.VerticalOrientation.ABOVE)
                        .build());
    }

    private void showToast(int resource) {
        if (mToast != null) {
            mToast.cancel();
        }
        String toastMessage = mActivity.getResources().getString(resource);
        mToast = Toast.makeText(mActivity, toastMessage, Toast.LENGTH_SHORT);
        mToast.setGravity(
                mToast.getGravity(),
                mToast.getXOffset(),
                mActivity.getResources().getDimensionPixelSize(R.dimen.y_offset_full_sharesheet));
        mToast.show();
    }

    private void centerIcon(ImageView imageView) {
        imageView.setScaleType(ScaleType.FIT_XY);
        int padding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.sharing_hub_preview_icon_padding);
        imageView.setPadding(padding, padding, padding, padding);
    }

    /**
     * Fetches the favicon for the given url.
     **/
    private void fetchFavicon(String url) {
        if (!url.isEmpty()) {
            mIconBridge.getLargeIconForUrl(
                    new GURL(url),
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.default_favicon_min_size),
                    this::onFaviconAvailable);
        }
    }

    /**
     * Passed as the callback to {@link LargeIconBridge#getLargeIconForStringUrl}
     * by showShareSheetWithMessage.
     */
    private void onFaviconAvailable(
            @Nullable Bitmap icon,
            @ColorInt int fallbackColor,
            boolean isColorDefault,
            @IconType int iconType) {
        int size =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.sharing_hub_preview_inner_icon_size);
        // If we didn't get a favicon, use the generic favicon instead.
        Bitmap scaledIcon;
        if (icon == null) {
            scaledIcon = FaviconUtils.createGenericFaviconBitmap(mActivity, size, null);
            RecordUserAction.record("SharingHubAndroid.GenericFaviconShown");
        } else {
            scaledIcon = Bitmap.createScaledBitmap(icon, size, size, true);
            // Align the bitmap density to match the context DisplayMetrics density. This is
            // particularly important on automotive, where the density is intentionally scaled up.
            scaledIcon.setDensity(mActivity.getResources().getDisplayMetrics().densityDpi);
            RecordUserAction.record("SharingHubAndroid.LinkFaviconShown");
        }
        ImageView imageView = this.getContentView().findViewById(R.id.image_preview);
        imageView.setImageBitmap(scaledIcon);
        centerIcon(imageView);
    }

    private String getFileType(String mimeType) {
        if (!mimeType.contains("/")) {
            return "";
        }
        String supertype = mimeType.split("/", 2)[0];
        // Accepted MIME types are drawn from //chrome/browser/webshare/share_service_impl.cc
        switch (supertype) {
            case "audio":
                return mActivity
                        .getResources()
                        .getString(R.string.sharing_hub_audio_preview_subtitle);
            case "image":
                return mActivity
                        .getResources()
                        .getString(R.string.sharing_hub_image_preview_subtitle);
            case "text":
                return mActivity
                        .getResources()
                        .getString(R.string.sharing_hub_text_preview_subtitle);
            case "video":
                return mActivity
                        .getResources()
                        .getString(R.string.sharing_hub_video_preview_subtitle);
            default:
                return "";
        }
    }

    /** One-shot reporter that records the first time the user scrolls a {@link RecyclerView}. */
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
        return R.string.sharing_hub_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.sharing_hub_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.sharing_hub_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.sharing_hub_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {}
}
