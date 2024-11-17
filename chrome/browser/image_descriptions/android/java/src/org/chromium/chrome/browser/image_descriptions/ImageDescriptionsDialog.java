// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.RadioGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

/**
 * Dialog for the "Get Image Descriptions" feature. If a user is a screen reader user, they will
 * see a new option under the main menu to get image descriptions. If they select that option this
 * dialog will display giving the user the option to enable the feature.
 */
public class ImageDescriptionsDialog
        implements ModalDialogProperties.Controller, RadioGroup.OnCheckedChangeListener {
    // Please treat this list as append only and keep it in sync with
    // AccessibilityImageLabelModeAndroid in enums.xml
    //
    // LINT.IfChange(ImageDescriptionsDialogAction)
    @IntDef({
        ImageDescriptionsDialogAction.ENABLED,
        ImageDescriptionsDialogAction.ENABLED_ONLY_ON_WIFI,
        ImageDescriptionsDialogAction.JUST_ONCE,
        ImageDescriptionsDialogAction.JUST_ONCE_DONT_ASK_AGAIN,
        ImageDescriptionsDialogAction.CANCEL
    })
    public @interface ImageDescriptionsDialogAction {
        int ENABLED = 0;
        int ENABLED_ONLY_ON_WIFI = 1;
        int JUST_ONCE = 2;
        int JUST_ONCE_DONT_ASK_AGAIN = 3;
        int CANCEL = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityImageLabelModeAndroid)

    private ImageDescriptionsControllerDelegate mControllerDelegate;

    private ModalDialogManager mModalDialogManager;
    private PropertyModel mPropertyModel;
    private WebContentsObserver mWebContentsObserver;

    private RadioButtonWithDescriptionLayout mRadioGroup;
    private RadioButtonWithDescription mOptionJustOnceRadioButton;
    private RadioButtonWithDescription mOptionAlwaysRadioButton;
    private CheckBox mOptionalCheckbox;

    private boolean mShouldShowDontAskAgainOption;
    private boolean mOnlyOnWifiState;
    private boolean mDontAskAgainState;
    private @DialogDismissalCause int mDismissalCause;
    private WebContents mWebContents;
    private Profile mProfile;
    private Context mContext;

    protected ImageDescriptionsDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            ImageDescriptionsControllerDelegate delegate,
            boolean shouldShowDontAskAgainOption,
            WebContents webContents) {
        mModalDialogManager = modalDialogManager;
        mControllerDelegate = delegate;
        mWebContents = webContents;
        mProfile = Profile.fromWebContents(webContents).getOriginalProfile();
        mContext = context;

        // Set initial state.
        mShouldShowDontAskAgainOption = shouldShowDontAskAgainOption;
        mOnlyOnWifiState = true;
        mDontAskAgainState = false;
        mDismissalCause = DialogDismissalCause.UNKNOWN;

        // Inflate our custom view layout for this dialog.
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View rootView = inflater.inflate(R.layout.image_descriptions_dialog, null);

        mRadioGroup = rootView.findViewById(R.id.image_descriptions_dialog_radio_button_group);
        mRadioGroup.setOnCheckedChangeListener(this);

        mOptionJustOnceRadioButton =
                rootView.findViewById(R.id.image_descriptions_dialog_radio_button_just_once);
        mOptionAlwaysRadioButton =
                rootView.findViewById(R.id.image_descriptions_dialog_radio_button_always);

        mOptionalCheckbox = rootView.findViewById(R.id.image_descriptions_dialog_check_box);
        mOptionalCheckbox.setOnCheckedChangeListener(
                (buttonView, isChecked) -> {
                    if (mOptionJustOnceRadioButton.isChecked()) {
                        mDontAskAgainState = isChecked;
                    } else {
                        mOnlyOnWifiState = isChecked;
                    }
                });

        // Dialog should start with "Just once" checked, and with optional checkbox as needed.
        mOptionJustOnceRadioButton.setChecked(true);
        if (mShouldShowDontAskAgainOption) {
            updateOptionalCheckbox(R.string.dont_ask_again, mDontAskAgainState);
        }

        // Create a |WebContentsObserver| to track changes in state for which we should
        // dismiss the dialog, such as navigation, and |mWebContents| being hidden or destroyed.
        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility != Visibility.VISIBLE) {
                            // Treat occlusion as switching tabs.
                            mDismissalCause = DialogDismissalCause.TAB_SWITCHED;
                            unregisterObserverAndDismiss();
                        }
                    }

                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        mDismissalCause = DialogDismissalCause.NAVIGATE;
                        unregisterObserverAndDismiss();
                    }

                    @Override
                    public void onTopLevelNativeWindowChanged(
                            @Nullable WindowAndroid windowAndroid) {
                        // Dismiss the dialog when the associated WebContents is detached from the
                        // window.
                        if (windowAndroid == null) {
                            mDismissalCause = DialogDismissalCause.NOT_ATTACHED_TO_WINDOW;
                            unregisterObserverAndDismiss();
                        }
                    }

                    @Override
                    public void destroy() {
                        super.destroy();
                        // If no dismissal cause has been set, web contents were destroyed.
                        if (mDismissalCause == DialogDismissalCause.UNKNOWN) {
                            mDismissalCause = DialogDismissalCause.WEB_CONTENTS_DESTROYED;
                        }
                        dismiss();
                    }
                };

        // Build our dialog property model.
        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getResources(),
                                R.string.image_descriptions_dialog_header)
                        .with(ModalDialogProperties.CUSTOM_VIEW, rootView)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                R.string.no_thanks)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                R.string.image_descriptions_dialog_get_descriptions_button)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        // When the "Always" option is checked, we display the choice for "Only on Wi-Fi", we
        // hide this checkbox when user has selected "Just once" and optionally display a
        // "Don't ask again" checkbox if the user has chosen just once enough times.
        if (checkedId == mOptionAlwaysRadioButton.getId()) {
            updateOptionalCheckbox(
                    R.string.image_descriptions_dialog_option_only_on_wifi, mOnlyOnWifiState);
        } else if (checkedId == mOptionJustOnceRadioButton.getId()) {
            if (mShouldShowDontAskAgainOption) {
                updateOptionalCheckbox(R.string.dont_ask_again, mDontAskAgainState);
            } else {
                mOptionalCheckbox.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        int toastMessage = -1;
        int userAction = -1;

        // User has elected to get image descriptions
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            // Determine desired level of descriptions and default to just once
            if (mOptionAlwaysRadioButton.isChecked()) {
                toastMessage = R.string.image_descriptions_toast_on;
                mControllerDelegate.enableImageDescriptions(mProfile);
                mControllerDelegate.setOnlyOnWifiRequirement(mOnlyOnWifiState, mProfile);

                userAction =
                        mOnlyOnWifiState
                                ? ImageDescriptionsDialogAction.ENABLED_ONLY_ON_WIFI
                                : ImageDescriptionsDialogAction.ENABLED;

                // If user requested "only on wifi" and we have no wifi, provide alt toast.
                if (mOnlyOnWifiState
                        && (DeviceConditions.getCurrentNetConnectionType(mContext)
                                != ConnectionType.CONNECTION_WIFI)) {
                    toastMessage = R.string.image_descriptions_toast_on_no_wifi;
                }
            } else if (mOptionJustOnceRadioButton.isChecked()) {
                mControllerDelegate.getImageDescriptionsJustOnce(mDontAskAgainState, mWebContents);
                toastMessage = R.string.image_descriptions_toast_just_once;

                userAction =
                        mDontAskAgainState
                                ? ImageDescriptionsDialogAction.JUST_ONCE_DONT_ASK_AGAIN
                                : ImageDescriptionsDialogAction.JUST_ONCE;
            }

            mDismissalCause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
        } else {
            mDismissalCause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
            userAction = ImageDescriptionsDialogAction.CANCEL;
        }

        // Make a toast, if necessary.
        if (toastMessage != -1) Toast.makeText(mContext, toastMessage, Toast.LENGTH_LONG).show();

        // On user action, record histogram metric.
        recordHistogramMetric(userAction);

        // Dismiss the dialog and unregister observer.
        unregisterObserverAndDismiss();
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {}

    /**
     * Helper method to set the optional checkbox text, visibility, and checked state
     * @param textId    ID of string for primary text
     * @param state     Checked state of optional checkbox
     */
    private void updateOptionalCheckbox(int textId, boolean state) {
        mOptionalCheckbox.setVisibility(View.VISIBLE);
        mOptionalCheckbox.setText(textId);
        mOptionalCheckbox.setChecked(state);
    }

    /**
     * Helper method to unregister |mWebContentsObserver| during changes in state to |mWebContents|
     * or on user action. The call to #destroy() will also dismiss the dialog.
     */
    private void unregisterObserverAndDismiss() {
        mWebContentsObserver.destroy();
    }

    /** Helper method to display this dialog. */
    public void show() {
        mModalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }

    /** Helper method to dismiss this dialog. Dismisses the dialog with cause |mDismissalCause|. */
    private void dismiss() {
        mModalDialogManager.dismissDialog(mPropertyModel, mDismissalCause);
    }

    /**
     * Helper method to record metrics for user choice when interacting with dialog.
     *
     * @param action action user has taken on the dialog
     */
    private void recordHistogramMetric(@ImageDescriptionsDialogAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Accessibility.ImageLabels.Android.DialogOption",
                action,
                ImageDescriptionsDialogAction.NUM_ENTRIES);
    }
}
