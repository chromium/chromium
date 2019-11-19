// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupMenu;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipViewHolder;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * This class is responsible for pushing updates to the Autofill Assistant header view. These
 * updates are pulled from the {@link AssistantHeaderModel} when a notification of an update is
 * received.
 */
class AssistantHeaderViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantHeaderModel,
                AssistantHeaderViewBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final Context mContext;
        final AnimatedPoodle mPoodle;
        final ViewGroup mHeader;
        final TextView mStatusMessage;
        final AnimatedProgressBar mProgressBar;
        final View mProfileIconView;
        final PopupMenu mProfileIconMenu;
        @Nullable
        AssistantChipViewHolder mChip;
        @Nullable
        TextBubble mTextBubble;

        ViewHolder(Context context, ViewGroup headerView, AnimatedPoodle poodle) {
            mContext = context;
            mPoodle = poodle;
            mHeader = headerView;
            mStatusMessage = headerView.findViewById(R.id.status_message);
            mProgressBar = new AnimatedProgressBar(headerView.findViewById(R.id.progress_bar));
            mProfileIconView = headerView.findViewById(R.id.profile_image);
            mProfileIconMenu = new PopupMenu(context, mProfileIconView);
            mProfileIconMenu.inflate(R.menu.profile_icon_menu);
            mProfileIconView.setOnClickListener(unusedView -> mProfileIconMenu.show());
        }

        @VisibleForTesting
        void disableAnimationsForTesting(boolean disable) {
            mProgressBar.disableAnimationsForTesting(disable);
        }
    }

    @Override
    public void bind(AssistantHeaderModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantHeaderModel.STATUS_MESSAGE == propertyKey) {
            String message = model.get(AssistantHeaderModel.STATUS_MESSAGE);
            view.mStatusMessage.setText(message);
            view.mStatusMessage.announceForAccessibility(message);
        } else if (AssistantHeaderModel.PROGRESS == propertyKey) {
            view.mProgressBar.setProgress(model.get(AssistantHeaderModel.PROGRESS));
        } else if (AssistantHeaderModel.PROGRESS_VISIBLE == propertyKey) {
            if (model.get(AssistantHeaderModel.PROGRESS_VISIBLE)) {
                view.mProgressBar.show();
            } else {
                view.mProgressBar.hide();
            }
        } else if (AssistantHeaderModel.SPIN_POODLE == propertyKey) {
            view.mPoodle.setSpinEnabled(model.get(AssistantHeaderModel.SPIN_POODLE));
        } else if (AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK == propertyKey) {
            setProfileMenuListener(view, model.get(AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK));
        } else if (AssistantHeaderModel.CHIP == propertyKey) {
            bindChip(view, model.get(AssistantHeaderModel.CHIP));
            maybeShowChip(model, view);
        } else if (AssistantHeaderModel.CHIP_VISIBLE == propertyKey) {
            maybeShowChip(model, view);
        } else if (AssistantHeaderModel.BUBBLE_MESSAGE == propertyKey) {
            showOrDismissBubble(model, view);
        } else {
            assert false : "Unhandled property detected in AssistantHeaderViewBinder!";
        }
    }

    private void maybeShowChip(AssistantHeaderModel model, ViewHolder view) {
        if (model.get(AssistantHeaderModel.CHIP_VISIBLE)
                && model.get(AssistantHeaderModel.CHIP) != null) {
            view.mChip.getView().setVisibility(View.VISIBLE);
            view.mProfileIconView.setVisibility(View.GONE);
        } else {
            if (view.mChip != null) {
                view.mChip.getView().setVisibility(View.GONE);
            }

            view.mProfileIconView.setVisibility(View.VISIBLE);
        }
    }

    private void bindChip(ViewHolder view, @Nullable AssistantChip chip) {
        if (chip == null) {
            return;
        }

        int viewType = AssistantChipViewHolder.getViewType(chip);

        // If there is already a chip in the header but with incompatible type, remove it.
        ViewGroup parent = (ViewGroup) view.mStatusMessage.getParent();
        if (view.mChip != null && view.mChip.getType() != viewType) {
            parent.removeView(view.mChip.getView());
            view.mChip = null;
        }

        // If there is no chip already in the header, create one and add it at the end of the
        // header.
        if (view.mChip == null) {
            view.mChip = AssistantChipViewHolder.create(view.mHeader, viewType);
            parent.addView(view.mChip.getView());
        }

        // Bind the chip to the view.
        view.mChip.bind(chip);
    }

    private void setProfileMenuListener(ViewHolder view, @Nullable Runnable feedbackCallback) {
        view.mProfileIconMenu.setOnMenuItemClickListener(item -> {
            int itemId = item.getItemId();
            if (itemId == R.id.settings) {
                PreferencesLauncher.launchSettingsPage(
                        view.mHeader.getContext(), AutofillAssistantPreferences.class);
                return true;
            } else if (itemId == R.id.send_feedback) {
                if (feedbackCallback != null) {
                    feedbackCallback.run();
                }
                return true;
            }

            return false;
        });
    }

    private void showOrDismissBubble(AssistantHeaderModel model, ViewHolder view) {
        String message = model.get(AssistantHeaderModel.BUBBLE_MESSAGE);
        if (message.isEmpty() && view.mTextBubble != null) {
            view.mTextBubble.dismiss();
            view.mTextBubble = null;
            return;
        }
        View poodle = view.mPoodle.getView();
        view.mTextBubble = new TextBubble(
                /*context = */ view.mContext, /*rootView = */ poodle, /*contentString = */ message,
                /*accessibilityString = */ message, /*showArrow = */ true,
                /*anchorRectProvider = */ new ViewRectProvider(poodle));
        view.mTextBubble.setDismissOnTouchInteraction(true);
        view.mTextBubble.show();
    }
}
