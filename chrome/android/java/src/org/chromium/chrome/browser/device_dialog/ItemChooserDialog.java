// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A general-purpose dialog for presenting a list of things to pick from.
 *
 * The dialog is shown by the ItemChooserDialog constructor, and always calls
 * ItemSelectedCallback.onItemSelected() as it's closing.
 */
public class ItemChooserDialog implements DeviceItemAdapter.Observer {
    /**
     * An interface to implement to get a callback when something has been
     * selected.
     */
    public interface ItemSelectedCallback {
        /**
         * Returns the user selection.
         *
         * @param id The id of the item selected. Blank if the dialog was closed
         * without selecting anything.
         */
        void onItemSelected(String id);
    }

    /**
     * The labels to show in the dialog.
     */
    public static class ItemChooserLabels {
        // The title at the top of the dialog.
        public final CharSequence title;
        // The message to show while there are no results.
        public final CharSequence searching;
        // The message to show when no results were produced.
        public final CharSequence noneFound;
        // A status message to show above the button row after an item has
        // been added and discovery is still ongoing.
        public final CharSequence statusActive;
        // A status message to show above the button row after discovery has
        // stopped and no devices have been found.
        public final CharSequence statusIdleNoneFound;
        // A status message to show above the button row after an item has
        // been added and discovery has stopped.
        public final CharSequence statusIdleSomeFound;
        // The label for the positive button (e.g. Select/Pair).
        public final CharSequence positiveButton;

        public ItemChooserLabels(CharSequence title, CharSequence searching, CharSequence noneFound,
                CharSequence statusActive, CharSequence statusIdleNoneFound,
                CharSequence statusIdleSomeFound, CharSequence positiveButton) {
            this.title = title;
            this.searching = searching;
            this.noneFound = noneFound;
            this.statusActive = statusActive;
            this.statusIdleNoneFound = statusIdleNoneFound;
            this.statusIdleSomeFound = statusIdleSomeFound;
            this.positiveButton = positiveButton;
        }
    }

    /**
     * The various states the dialog can represent.
     */
    @IntDef({State.INITIALIZING_ADAPTER, State.STARTING, State.PROGRESS_UPDATE_AVAILABLE,
            State.DISCOVERY_IDLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int INITIALIZING_ADAPTER = 0;
        int STARTING = 1;
        int PROGRESS_UPDATE_AVAILABLE = 2;
        int DISCOVERY_IDLE = 3;
    }

    private Activity mActivity;

    // The dialog this class encapsulates.
    private Dialog mDialog;

    // The callback to notify when the user selected an item.
    private ItemSelectedCallback mItemSelectedCallback;

    // Individual UI elements.
    private TextViewWithClickableSpans mTitle;
    private TextViewWithClickableSpans mEmptyMessage;
    private ProgressBar mProgressBar;
    private ListView mListView;
    private TextView mStatus;
    private Button mConfirmButton;

    // The labels to display in the dialog.
    private ItemChooserLabels mLabels;

    // The adapter containing the items to show in the dialog.
    private DeviceItemAdapter mItemAdapter;

    // How much of the height of the screen should be taken up by the listview.
    private static final float LISTVIEW_HEIGHT_PERCENT = 0.30f;
    // The height of a row of the listview in dp.
    private static final int LIST_ROW_HEIGHT_DP = 48;
    // The minimum height of the listview in the dialog (in dp).
    private static final int MIN_HEIGHT_DP = (int) (LIST_ROW_HEIGHT_DP * 1.5);
    // The maximum height of the listview in the dialog (in dp).
    private static final int MAX_HEIGHT_DP = (int) (LIST_ROW_HEIGHT_DP * 8.5);

    // If this variable is false, the window should be closed when it loses focus;
    // Otherwise, the window should not be closed when it loses focus.
    private boolean mIgnorePendingWindowFocusChangeForClose;

    /**
     * Creates the ItemChooserPopup and displays it (and starts waiting for data).
     *
     * @param activity Activity which is used for launching a dialog.
     * @param callback The callback used to communicate back what was selected.
     * @param labels The labels to show in the dialog.
     */
    public ItemChooserDialog(
            Activity activity, ItemSelectedCallback callback, ItemChooserLabels labels) {
        mActivity = activity;
        mItemSelectedCallback = callback;
        mLabels = labels;

        LinearLayout dialogContainer = (LinearLayout) LayoutInflater.from(mActivity).inflate(
                R.layout.item_chooser_dialog, null);

        mListView = (ListView) dialogContainer.findViewById(R.id.items);
        mProgressBar = (ProgressBar) dialogContainer.findViewById(R.id.progress);
        mStatus = (TextView) dialogContainer.findViewById(R.id.status);
        mTitle = (TextViewWithClickableSpans) dialogContainer.findViewById(
                R.id.dialog_title);
        mEmptyMessage =
                (TextViewWithClickableSpans) dialogContainer.findViewById(R.id.not_found_message);

        mTitle.setText(labels.title);
        mTitle.setMovementMethod(LinkMovementMethod.getInstance());

        mEmptyMessage.setMovementMethod(LinkMovementMethod.getInstance());
        mStatus.setMovementMethod(LinkMovementMethod.getInstance());

        mConfirmButton = (Button) dialogContainer.findViewById(R.id.positive);
        mConfirmButton.setText(labels.positiveButton);
        mConfirmButton.setEnabled(false);

        View.OnClickListener clickListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mItemSelectedCallback.onItemSelected(mItemAdapter.getSelectedItemKey());
                mDialog.setOnDismissListener(null);
                mDialog.dismiss();
            }
        };

        mItemAdapter = new DeviceItemAdapter(
                mActivity, /*itemsSelectable=*/true, R.layout.item_chooser_dialog_row);
        mItemAdapter.setNotifyOnChange(true);
        mItemAdapter.setObserver(this);

        mConfirmButton.setOnClickListener(clickListener);
        mListView.setOnItemClickListener(mItemAdapter);

        mListView.setAdapter(mItemAdapter);
        mListView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        mListView.setEmptyView(mEmptyMessage);
        mListView.setDivider(null);
        setState(State.STARTING);

        mIgnorePendingWindowFocusChangeForClose = false;

        showDialogForView(dialogContainer);

        dialogContainer.addOnLayoutChangeListener(
                (View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) -> {
                    if (l != ol || t != ot || r != or || b != ob) {
                        // The list is the main element in the dialog and it should grow and
                        // shrink according to the size of the screen available.
                        View listViewContainer = dialogContainer.findViewById(R.id.container);
                        listViewContainer.setLayoutParams(new LinearLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT,
                                getListHeight(mActivity.getWindow().getDecorView().getHeight(),
                                        mActivity.getResources().getDisplayMetrics().density)));
                    }
                });
    }

    // DeviceItemAdapter.Observer:
    @Override
    public void onItemSelectionChanged(boolean itemSelected) {
        mConfirmButton.setEnabled(itemSelected);
    }

    /**
     * Sets whether the window should be closed when it loses focus.
     *
     * @param ignorePendingWindowFocusChangeForClose Whether the window should be closed when it
     * loses focus.
     */
    public void setIgnorePendingWindowFocusChangeForClose(
            boolean ignorePendingWindowFocusChangeForClose) {
        mIgnorePendingWindowFocusChangeForClose = ignorePendingWindowFocusChangeForClose;
    }

    // Computes the height of the device list, bound to half-multiples of the
    // row height so that it's obvious if there are more elements to scroll to.
    @VisibleForTesting
    static int getListHeight(int decorHeight, float density) {
        float heightDp = decorHeight / density * LISTVIEW_HEIGHT_PERCENT;
        // Round to (an integer + 0.5) times LIST_ROW_HEIGHT.
        heightDp = (Math.round(heightDp / LIST_ROW_HEIGHT_DP - 0.5f) + 0.5f) * LIST_ROW_HEIGHT_DP;
        heightDp = MathUtils.clamp(heightDp, MIN_HEIGHT_DP, MAX_HEIGHT_DP);
        return Math.round(heightDp * density);
    }

    private void showDialogForView(View view) {
        mDialog = new Dialog(mActivity) {
            @Override
            public void onWindowFocusChanged(boolean hasFocus) {
                super.onWindowFocusChanged(hasFocus);
                if (!mIgnorePendingWindowFocusChangeForClose && !hasFocus) super.dismiss();
                setIgnorePendingWindowFocusChangeForClose(false);
            }
        };
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.setCanceledOnTouchOutside(true);
        mDialog.addContentView(view,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                              LinearLayout.LayoutParams.MATCH_PARENT));
        mDialog.setOnCancelListener(dialog -> mItemSelectedCallback.onItemSelected(""));

        Window window = mDialog.getWindow();
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            // On smaller screens, make the dialog fill the width of the screen,
            // and appear at the top.
            window.setBackgroundDrawable(new ColorDrawable(Color.WHITE));
            window.setGravity(Gravity.TOP);
            window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                             ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        mDialog.show();
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    /**
     * Adds an item to the end of the list to show in the dialog if the item
     * was not in the chooser. Otherwise updates the items description.
     *
     * @param key Unique identifier for that item.
     * @param description Text in the row.
     */
    public void addOrUpdateItem(String key, String description) {
        addOrUpdateItem(key, description, null /* icon */, null /* iconDescription */);
    }

    /**
     * Adds an item to the end of the list to show in the dialog if the item
     * was not in the chooser. Otherwise updates the items description or icon.
     * Note that as long as at least one item has an icon all rows will be inset
     * with the icon dimensions.
     *
     * @param key Unique identifier for that item.
     * @param description Text in the row.
     * @param icon Drawable to show left of the description. The drawable provided should
     *        be stateful and handle the selected state to be rendered correctly.
     * @param iconDescription Description of the icon.
     */
    public void addOrUpdateItem(String key, String description, @Nullable Drawable icon,
            @Nullable String iconDescription) {
        mProgressBar.setVisibility(View.GONE);
        mItemAdapter.addOrUpdate(key, description, icon, iconDescription);
        setState(State.PROGRESS_UPDATE_AVAILABLE);
    }

    /**
     * Removes an item that is shown in the dialog.
     *
     * @param key Unique identifier for the item.
     */
    public void removeItemFromList(String key) {
        mItemAdapter.removeItemWithKey(key);
        setState(State.DISCOVERY_IDLE);
    }

    /**
     * Indicates the chooser that no more items will be added.
     */
    public void setIdleState() {
        mProgressBar.setVisibility(View.GONE);
        setState(State.DISCOVERY_IDLE);
    }

    /**
     * Indicates the adapter is being initialized.
     */
    public void signalInitializingAdapter() {
        setState(State.INITIALIZING_ADAPTER);
    }

    /**
     * Clear all items from the dialog.
     */
    public void clear() {
        mItemAdapter.clear();
        setState(State.STARTING);
    }

    /**
     * Shows an error message in the dialog.
     */
    public void setErrorState(CharSequence errorMessage, CharSequence errorStatus) {
        mListView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.GONE);
        mEmptyMessage.setText(errorMessage);
        mEmptyMessage.setVisibility(View.VISIBLE);
        mStatus.setText(errorStatus);
    }

    private void setState(@State int state) {
        switch (state) {
            case State.STARTING:
                mStatus.setText(mLabels.searching);
            // fall through
            case State.INITIALIZING_ADAPTER:
                mListView.setVisibility(View.GONE);
                mProgressBar.setVisibility(View.VISIBLE);
                mEmptyMessage.setVisibility(View.GONE);
                break;
            case State.PROGRESS_UPDATE_AVAILABLE:
                mStatus.setText(mLabels.statusActive);
                mProgressBar.setVisibility(View.GONE);
                mListView.setVisibility(View.VISIBLE);
                break;
            case State.DISCOVERY_IDLE:
                boolean showEmptyMessage = mItemAdapter.isEmpty();
                mStatus.setText(showEmptyMessage
                        ? mLabels.statusIdleNoneFound : mLabels.statusIdleSomeFound);
                mEmptyMessage.setText(mLabels.noneFound);
                mEmptyMessage.setVisibility(showEmptyMessage ? View.VISIBLE : View.GONE);
                break;
        }
    }

    /**
     * Returns the dialog associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public Dialog getDialogForTesting() {
        return mDialog;
    }

    /**
     * Returns the ItemAdapter associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public DeviceItemAdapter getItemAdapterForTesting() {
        return mItemAdapter;
    }
}
