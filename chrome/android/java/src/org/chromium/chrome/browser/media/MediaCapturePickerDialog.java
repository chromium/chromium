// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.media.MediaCapturePickerHeadlessFragment.CaptureAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
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
    private final LinearLayout mButtonsView;
    private final View mPositiveButton;
    private final View mScreenButton;
    private final MaterialSwitch mAudioSwitch;
    private final ModelList mModelList = new ModelList();
    private final Map<Tab, TabItemState> mTabItemStateMap = new HashMap<>();
    @Nullable private TabItemState mLastSelectedTabItemState;
    @Nullable private PropertyModel mPropertyModel;
    @Nullable private Delegate mDelegate;

    /** A delegate for handling returning the picker result. */
    interface Delegate {
        /**
         * Called when the user has selected a tab to share.
         *
         * @param webContents The contents to share.
         * @param audioShare True if tab audio should be shared.
         */
        void onPickTab(@NonNull WebContents webContents, boolean audioShare);

        /** Called when the user has selected a window to share. */
        void onPickWindow();

        /** Called when the user has selected a screen to share. */
        void onPickScreen();

        /** Called when the user has elected to not share anything. */
        void onCancel();
    }

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
            mPositiveButton.setEnabled(true);
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
     * @param appName Name of the app that wants to share content.
     * @param requestAudio True if audio sharing is also requested.
     * @param delegate Invoked with a WebContents if a tab is selected, or {@code null} if the
     *     dialog is dismissed.
     */
    public static void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            String appName,
            boolean requestAudio,
            Delegate delegate) {
        new MediaCapturePickerDialog(context, modalDialogManager, appName, requestAudio, delegate)
                .show();
    }

    private MediaCapturePickerDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            String appName,
            boolean requestAudio,
            Delegate delegate) {
        mModalDialogManager = modalDialogManager;
        mAppName = appName;
        mDelegate = delegate;

        mDialogView =
                LayoutInflater.from(context).inflate(R.layout.media_capture_picker_dialog, null);

        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        adapter.registerType(
                EntryType.DEFAULT,
                parentView ->
                        LayoutInflater.from(context)
                                .inflate(R.layout.media_capture_picker_list_item, null),
                MediaCapturePickerItemViewBinder::bind);

        ListView listView = mDialogView.findViewById(R.id.media_capture_picker_list_view);
        listView.setAdapter(adapter);

        mButtonsView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.media_capture_picker_button_row, null);

        mPositiveButton = mButtonsView.findViewById(R.id.positive_button);
        mScreenButton = mButtonsView.findViewById(R.id.screen_button);

        // Share audio should be on by default.
        mAudioSwitch = mButtonsView.findViewById(R.id.media_capture_picker_audio_share_switch);

        if (requestAudio) {
            // Share audio should be on by default if audio sharing was requested.
            mAudioSwitch.setChecked(true);
        } else {
            mButtonsView
                    .findViewById(R.id.media_capture_picker_audio_share_row)
                    .setVisibility(View.GONE);
        }

        View audioShareRow = mButtonsView.findViewById(R.id.media_capture_picker_audio_share_row);
        audioShareRow.setOnClickListener((v) -> mAudioSwitch.toggle());
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

    private void startAndroidCapturePrompt() {
        var fragment = MediaCapturePickerHeadlessFragment.getInstanceForCurrentActivity();
        fragment.startAndroidCapturePrompt(
                result -> {
                    switch (result) {
                        case CaptureAction.CAPTURE_CANCELLED:
                            mDelegate.onCancel();
                            break;
                        case CaptureAction.CAPTURE_WINDOW:
                            mDelegate.onPickWindow();
                            break;
                        case CaptureAction.CAPTURE_SCREEN:
                            mDelegate.onPickScreen();
                            break;
                        default:
                            assert false;
                    }

                    mDelegate = null;
                    mModalDialogManager.dismissDialog(
                            mPropertyModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
                });
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

                            mDelegate.onPickTab(webContents, mAudioSwitch.isChecked());
                        } else {
                            mDelegate.onCancel();
                        }
                        mDelegate = null;
                        mModalDialogManager.dismissDialog(
                                model,
                                picked
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (mDelegate != null) {
                            mDelegate.onCancel();
                            mDelegate = null;
                        }
                        allTabObserver.destroy();
                    }
                };

        Resources resources = mDialogView.getResources();
        var title = resources.getString(R.string.media_capture_picker_dialog_title, mAppName);

        assert mPropertyModel == null;
        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mButtonsView)
                        .build();

        mButtonsView
                .findViewById(R.id.negative_button)
                .setOnClickListener(
                        view -> controller.onClick(mPropertyModel, ButtonType.NEGATIVE));

        mPositiveButton.setOnClickListener(
                view -> controller.onClick(mPropertyModel, ButtonType.POSITIVE));

        mScreenButton.setOnClickListener(view -> startAndroidCapturePrompt());

        // TODO(crbug.com/352187279): Show button once the entire screen sharing pipeline is
        // working.
        mScreenButton.setVisibility(View.GONE);

        mModalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
