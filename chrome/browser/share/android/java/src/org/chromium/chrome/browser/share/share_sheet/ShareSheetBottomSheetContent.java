// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.share.share_sheet;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * Bottom sheet content to display a 2-row custom share sheet.
 */
class ShareSheetBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private static final int SHARE_SHEET_ITEM = 0;
    private final Context mContext;
    private final ShareSheetCoordinator mShareSheetCoordinator;
    private ViewGroup mToolbarView;
    private ViewGroup mContentView;
    private ShareParams mParams;

    /**
     * Creates a ShareSheetBottomSheetContent (custom share sheet) opened from the given activity.
     *
     * @param context The context the share sheet was launched from.
     * @param shareSheetCoordinator The Cooredinator that instatiated this BottomSheetContent.
     */
    ShareSheetBottomSheetContent(
            Context context, ShareSheetCoordinator shareSheetCoordinator, ShareParams params) {
        mContext = context;
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
     * @param message The message to show on top of the share sheet.
     */
    void createRecyclerViews(List<PropertyModel> firstPartyModels,
            List<PropertyModel> thirdPartyModels, String message) {
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
            TextView titleView = this.getContentView().findViewById(R.id.title_preview);
            titleView.setText(mParams.getTitle());
            TextView urlView = this.getContentView().findViewById(R.id.url_preview);
            urlView.setText(mParams.getUrl());
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

    void setFaviconForPreview(Bitmap icon) {
        ImageView imageView = this.getContentView().findViewById(R.id.image_preview);
        imageView.setImageBitmap(icon);
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
