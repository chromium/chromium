// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Dialog for selecting a media source for media capture. */
public class MediaCapturePickerDialog implements AllTabObserver.Observer {
    private final ModalDialogManager mModalDialogManager;
    private final String mAppName;
    private final View mDialogView;
    private final ModelList mModelList = new ModelList();
    private final Map<Tab, TabItemState> mTabItemStateMap = new HashMap<>();
    @Nullable private TabItemState mLastSelectedTabItemState;
    @Nullable private Callback<WebContents> mCallback;

    private class TabItemState {
        private final Tab mTab;
        private final ModelListAdapter.ListItem mItem;
        private final PropertyModel mModel;

        TabItemState(Tab tab) {
            mTab = tab;
            mModel =
                    new PropertyModel.Builder(MediaCapturePickerItemProperties.ALL_KEYS)
                            .with(MediaCapturePickerItemProperties.CLICK_LISTENER, this::onClick)
                            .with(MediaCapturePickerItemProperties.TAB_NAME, tab.getTitle())
                            .with(MediaCapturePickerItemProperties.SELECTED, false)
                            .build();
            mItem = new ModelListAdapter.ListItem(EntryType.DEFAULT, mModel);
            mModelList.add(mItem);
        }

        private void onClick(View view) {
            if (mLastSelectedTabItemState != null) {
                mLastSelectedTabItemState.mModel.set(
                        MediaCapturePickerItemProperties.SELECTED, false);
            }
            mModel.set(MediaCapturePickerItemProperties.SELECTED, true);
            mLastSelectedTabItemState = this;
        }

        void destroy() {
            mModelList.remove(mItem);
            if (mLastSelectedTabItemState == this) {
                mLastSelectedTabItemState = null;
            }
        }
    }

    /** Type of the entries shown on the dialog. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MediaCapturePickerDialog.EntryType.DEFAULT})
    private @interface EntryType {
        int DEFAULT = 0;
    }

    /**
     * Shows the media capture picker dialog.
     *
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param callback Invoked with a WebContents if a tab is selected, or {@code null} if the
     *     dialog is dismissed.
     */
    public static void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            String appName,
            Callback<WebContents> callback) {
        new MediaCapturePickerDialog(context, modalDialogManager, appName, callback).show();
    }

    private MediaCapturePickerDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            String appName,
            Callback<WebContents> callback) {
        mModalDialogManager = modalDialogManager;
        mAppName = appName;
        mCallback = callback;

        mDialogView =
                LayoutInflater.from(context).inflate(R.layout.media_capture_picker_dialog, null);

        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        adapter.registerType(
                EntryType.DEFAULT,
                parentView ->
                        LayoutInflater.from(context)
                                .inflate(R.layout.media_capture_picker_list_item, null),
                MediaCapturePickerItemViewBinder::bind);

        ListView listView =
                (ListView) mDialogView.findViewById(R.id.media_capture_picker_list_view);
        listView.setAdapter(adapter);
    }

    @Override
    public void onTabAdded(Tab tab) {
        mTabItemStateMap.put(tab, new TabItemState(tab));
    }

    @Override
    public void onTabRemoved(Tab tab) {
        var removed = mTabItemStateMap.remove(tab);
        assert removed != null;
        removed.destroy();
    }

    private void show() {
        var allTabObserver = new AllTabObserver(this);

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        boolean picked = buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        if (picked && mLastSelectedTabItemState != null) {
                            var tab = mLastSelectedTabItemState.mTab;
                            tab.loadIfNeeded(TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER);
                            var webContents = tab.getWebContents();
                            assert webContents != null;
                            mCallback.onResult(webContents);
                        } else {
                            mCallback.onResult(null);
                        }
                        mCallback = null;
                        mModalDialogManager.dismissDialog(
                                model,
                                picked
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (mCallback != null) {
                            mCallback.onResult(null);
                            mCallback = null;
                        }
                        allTabObserver.destroy();
                    }
                };

        Resources resources = mDialogView.getResources();
        var title = resources.getString(R.string.media_capture_picker_dialog_title, mAppName);
        var propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.media_capture_picker_dialog_share_text)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .build();

        mModalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
