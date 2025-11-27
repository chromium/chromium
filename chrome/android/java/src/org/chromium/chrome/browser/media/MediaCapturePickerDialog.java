// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.annotation.IntDef;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.media.MediaCapturePickerHeadlessFragment.CaptureAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.content_public.browser.media.capture.ScreenCapture;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Dialog for selecting a media source for media capture. */
@NullMarked
public class MediaCapturePickerDialog implements MediaCapturePickerTabObserver.Delegate {
    // This web contents is the one that is receiving the shared content.
    private final ModalDialogManager mModalDialogManager;
    private final MediaCapturePickerManager.Params mParams;
    private final View mDialogView;
    private final LinearLayout mButtonsView;
    private final View mPositiveButton;
    private final View mScreenButton;
    private final MaterialSwitch mAudioSwitch;
    private final ModelList mModelList = new ModelList();
    private final Map<Tab, TabItemState> mTabItemStateMap = new HashMap<>();
    private @Nullable TabItemState mLastSelectedTabItemState;
    private @Nullable PropertyModel mPropertyModel;
    private MediaCapturePickerManager.@Nullable Delegate mDelegate;

    private class TabItemState {
        private final Tab mTab;
        private final MVCListAdapter.ListItem mItem;
        private final PropertyModel mModel;

        TabItemState(Tab tab) {
            mTab = tab;
            mModel =
                    new PropertyModel.Builder(MediaCapturePickerItemProperties.ALL_KEYS)
                            .with(MediaCapturePickerItemProperties.CLICK_LISTENER, this::onClick)
                            .with(MediaCapturePickerItemProperties.TAB_NAME, tab.getTitle())
                            .with(MediaCapturePickerItemProperties.SELECTED, false)
                            .build();
            mItem = new MVCListAdapter.ListItem(EntryType.DEFAULT, mModel);
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

        void update() {
            mModel.set(MediaCapturePickerItemProperties.TAB_NAME, mTab.getTitle());
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

    MediaCapturePickerDialog(
            Context context,
            MediaCapturePickerManager.Params params,
            MediaCapturePickerManager.Delegate delegate) {
        // TODO(crbug.com/352187279): Support all parameters in `params`.
        mParams = params;
        mModalDialogManager = ((ModalDialogManagerHolder) context).getModalDialogManager();
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

        if (params.captureThisTab) {
            mScreenButton.setVisibility(View.GONE);
        }

        // Share audio should be on by default.
        mAudioSwitch = mButtonsView.findViewById(R.id.media_capture_picker_audio_share_switch);

        if (params.requestAudio) {
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

    @Override
    public void onTabUpdated(Tab tab) {
        final var state = mTabItemStateMap.get(tab);
        assert state != null;
        state.update();
    }

    private void startAndroidCapturePrompt() {
        var fragment = MediaCapturePickerHeadlessFragment.getInstanceForCurrentActivity();
        assumeNonNull(fragment);
        fragment.startAndroidCapturePrompt(
                (action, result) -> {
                    if (action != CaptureAction.CAPTURE_CANCELLED) {
                        ScreenCapture.onPick(mParams.webContents, result);
                    }

                    assumeNonNull(mDelegate);
                    switch (action) {
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

    void show() {
        final var observer =
                new MediaCapturePickerTabObserver(this, mParams, assumeNonNull(mDelegate));
        final var allTabObserver = new AllTabObserver(observer);

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(@Nullable PropertyModel model, int buttonType) {
                        boolean picked = buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        assumeNonNull(mDelegate);
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
                        observer.destroy();
                    }
                };

        Resources resources = mDialogView.getResources();
        var title =
                resources.getString(R.string.media_capture_picker_dialog_title, mParams.appName);

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

        mModalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
