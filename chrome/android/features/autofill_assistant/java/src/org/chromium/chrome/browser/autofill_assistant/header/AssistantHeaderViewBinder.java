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
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferenceFragment;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipAdapter;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.content_public.browser.UiThreadTaskTraits;
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
    /** The amount of space to put between the top of the sheet and the bottom of the bubble.*/
    private static final int TEXT_BUBBLE_PIXELS_ABOVE_SHEET = 4;

    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final Context mContext;
        final AnimatedPoodle mPoodle;
        final ViewGroup mHeader;
        final TextView mStatusMessage;
        final AnimatedProgressBar mProgressBar;
        final AssistantStepProgressBar mStepProgressBar;
        final View mProfileIconView;
        final PopupMenu mProfileIconMenu;
        final RecyclerView mChipsContainer;
        @Nullable
        TextBubble mTextBubble;

        ViewHolder(Context context, ViewGroup headerView, AnimatedPoodle poodle,
                RecyclerView chipsContainer) {
            mContext = context;
            mPoodle = poodle;
            mHeader = headerView;
            mStatusMessage = headerView.findViewById(R.id.status_message);
            mProgressBar = new AnimatedProgressBar(headerView.findViewById(R.id.progress_bar));
            mStepProgressBar =
                    new AssistantStepProgressBar(headerView.findViewById(R.id.step_progress_bar));
            mProfileIconView = headerView.findViewById(R.id.profile_image);
            mProfileIconMenu = new PopupMenu(context, mProfileIconView);
            mProfileIconMenu.inflate(R.menu.profile_icon_menu);
            mProfileIconView.setOnClickListener(unusedView -> mProfileIconMenu.show());
            mChipsContainer = chipsContainer;
        }

        void disableAnimations(boolean disable) {
            mProgressBar.disableAnimations(disable);
            mStepProgressBar.disableAnimations(disable);
            // Hiding the animated poodle seems to be the easiest way to disable its animation since
            // {@link LogoView#setAnimationEnabled(boolean)} is private.
            mPoodle.getView().setVisibility(View.INVISIBLE);
            ((DefaultItemAnimator) mChipsContainer.getItemAnimator())
                    .setSupportsChangeAnimations(!disable);
        }

        void updateProgressBarVisibility(boolean visible, boolean useStepProgressBar) {
            if (visible && !useStepProgressBar) {
                mProgressBar.show();
            } else {
                mProgressBar.hide();
            }

            mStepProgressBar.setVisible(visible && useStepProgressBar);
        }
    }

    @Override
    public void bind(AssistantHeaderModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantHeaderModel.STATUS_MESSAGE == propertyKey) {
            String message = model.get(AssistantHeaderModel.STATUS_MESSAGE);
            AssistantTextUtils.applyVisualAppearanceTags(view.mStatusMessage, message, null);
            view.mStatusMessage.announceForAccessibility(view.mStatusMessage.getText());
        } else if (AssistantHeaderModel.PROGRESS == propertyKey) {
            view.mProgressBar.setProgress(model.get(AssistantHeaderModel.PROGRESS));
        } else if (AssistantHeaderModel.PROGRESS_ACTIVE_STEP == propertyKey) {
            int activeStep = model.get(AssistantHeaderModel.PROGRESS_ACTIVE_STEP);
            if (activeStep >= 0) {
                view.mStepProgressBar.setActiveStep(activeStep);
            }
        } else if (AssistantHeaderModel.PROGRESS_BAR_ERROR == propertyKey) {
            view.mStepProgressBar.setError(model.get(AssistantHeaderModel.PROGRESS_BAR_ERROR));
        } else if (AssistantHeaderModel.PROGRESS_VISIBLE == propertyKey
                || AssistantHeaderModel.USE_STEP_PROGRESS_BAR == propertyKey) {
            view.updateProgressBarVisibility(model.get(AssistantHeaderModel.PROGRESS_VISIBLE),
                    model.get(AssistantHeaderModel.USE_STEP_PROGRESS_BAR));
        } else if (AssistantHeaderModel.STEP_PROGRESS_BAR_ICONS == propertyKey) {
            view.mStepProgressBar.setSteps(model.get(AssistantHeaderModel.STEP_PROGRESS_BAR_ICONS));
            view.mStepProgressBar.disableAnimations(
                    model.get(AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING));
        } else if (AssistantHeaderModel.SPIN_POODLE == propertyKey) {
            view.mPoodle.setSpinEnabled(model.get(AssistantHeaderModel.SPIN_POODLE));
        } else if (AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK == propertyKey) {
            setProfileMenuListener(view, model.get(AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK));
        } else if (AssistantHeaderModel.CHIPS == propertyKey) {
            view.mChipsContainer.invalidateItemDecorations();
            ((AssistantChipAdapter) view.mChipsContainer.getAdapter())
                    .setChips(model.get(AssistantHeaderModel.CHIPS));
            maybeShowChips(model, view);
        } else if (AssistantHeaderModel.CHIPS_VISIBLE == propertyKey) {
            maybeShowChips(model, view);
        } else if (AssistantHeaderModel.BUBBLE_MESSAGE == propertyKey) {
            showOrDismissBubble(model, view);
        } else if (AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING == propertyKey) {
            view.disableAnimations(model.get(AssistantHeaderModel.DISABLE_ANIMATIONS_FOR_TESTING));
        } else {
            assert false : "Unhandled property detected in AssistantHeaderViewBinder!";
        }
    }

    private void maybeShowChips(AssistantHeaderModel model, ViewHolder view) {
        // The PostTask is necessary as a workaround for the sticky button occasionally not showing,
        // this makes sure that the change happens after any possibly clashing animation currently
        // happening.
        // TODO(b/164389932): Figure out a better fix that doesn't require issuing the change in the
        // following UI iteration.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (model.get(AssistantHeaderModel.CHIPS_VISIBLE)
                    && !model.get(AssistantHeaderModel.CHIPS).isEmpty()) {
                view.mChipsContainer.setVisibility(View.VISIBLE);
                view.mProfileIconView.setVisibility(View.GONE);
            } else {
                view.mChipsContainer.setVisibility(View.GONE);
                view.mProfileIconView.setVisibility(View.VISIBLE);
            }
        });
    }

    private void setProfileMenuListener(ViewHolder view, @Nullable Runnable feedbackCallback) {
        view.mProfileIconMenu.setOnMenuItemClickListener(item -> {
            int itemId = item.getItemId();
            if (itemId == R.id.settings) {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(
                        view.mHeader.getContext(), AutofillAssistantPreferenceFragment.class);
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
        if (message.isEmpty() && view.mTextBubble == null) {
            return;
        }
        if (message.isEmpty() && view.mTextBubble != null) {
            view.mTextBubble.dismiss();
            view.mTextBubble = null;
            return;
        }
        View poodle = view.mPoodle.getView();
        ViewRectProvider anchorRectProvider = new ViewRectProvider(poodle);
        int topOffset = view.mContext.getResources().getDimensionPixelSize(
                                R.dimen.autofill_assistant_root_view_top_padding)
                + TEXT_BUBBLE_PIXELS_ABOVE_SHEET;
        anchorRectProvider.setInsetPx(0, -topOffset, 0, 0);
        view.mTextBubble = new TextBubble(
                /*context = */ view.mContext, /*rootView = */ poodle, /*contentString = */ message,
                /*accessibilityString = */ message, /*showArrow = */ true,
                /*anchorRectProvider = */ anchorRectProvider,
                ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        view.mTextBubble.setDismissOnTouchInteraction(true);
        view.mTextBubble.show();
    }
}
