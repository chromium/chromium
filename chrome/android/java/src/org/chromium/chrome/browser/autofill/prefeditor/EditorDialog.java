// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.InputFilter;
import android.text.Spanned;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.settings.CreditCardNumberFormattingTextWatcher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldView;
import org.chromium.components.autofill.prefeditor.EditorObserverForTest;
import org.chromium.components.autofill.prefeditor.EditorTextField;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.AlwaysDismissedDialog;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView.EdgeType;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

/**
 * The editor dialog. Can be used for editing contact information, shipping address,
 * billing address, and credit cards.
 *
 * TODO(https://crbug.com/799905): Move payment specific functionality to separate class.
 */
public class EditorDialog
        extends AlwaysDismissedDialog implements OnClickListener, DialogInterface.OnShowListener,
                                                 DialogInterface.OnDismissListener {
    /** The indicator for input fields that are required. */
    public static final String REQUIRED_FIELD_INDICATOR = "*";

    /** Duration of the animation to show the UI to full height. */
    private static final int DIALOG_ENTER_ANIMATION_MS = 300;

    /** Duration of the animation to hide the UI. */
    private static final int DIALOG_EXIT_ANIMATION_MS = 195;

    private static EditorObserverForTest sObserverForTest;

    private final Activity mActivity;
    private final Handler mHandler;
    private final TextView.OnEditorActionListener mEditorActionListener;
    private final int mHalfRowMargin;
    private final List<EditorFieldView> mFieldViews;
    private final List<EditText> mEditableTextFields;
    private final List<Spinner> mDropdownFields;
    private final InputFilter mCardNumberInputFilter;
    private final TextWatcher mCardNumberFormatter;
    private final boolean mHasRequiredIndicator;

    @Nullable
    private TextWatcher mPhoneFormatter;
    private View mLayout;
    private EditorModel mEditorModel;
    private Button mDoneButton;
    private boolean mFormWasValid;
    private boolean mShouldTriggerDoneCallbackBeforeCloseAnimation;
    private ViewGroup mDataView;
    private View mFooter;
    @Nullable
    private TextView mCardInput;
    @Nullable
    private TextView mPhoneInput;

    private Animator mDialogInOutAnimator;
    @Nullable
    private Runnable mDeleteRunnable;
    private boolean mIsDismissed;
    private Profile mProfile;
    @Nullable
    private UiConfig mUiConfig;
    @Nullable
    private AlertDialog mConfirmationDialog;

    /**
     * Builds the editor dialog with the required indicator enabled.
     *
     * @param activity          The activity on top of which the UI should be displayed.
     * @param deleteRunnable    The runnable that when called will delete the profile.
     * @param profile           The current profile that creates EditorDialog.
     */
    public EditorDialog(Activity activity, Runnable deleteRunnable, Profile profile) {
        this(activity, deleteRunnable, profile, true);
    }

    /**
     * Builds the editor dialog.
     *
     * @param activity             The activity on top of which the UI should be displayed.
     * @param deleteRunnable       The runnable that when called will delete the profile.
     * @param profile              The current profile that creates EditorDialog.
     * @param hasRequiredIndicator Whether the required (*) indicator is visible.
     */
    public EditorDialog(Activity activity, Runnable deleteRunnable, Profile profile,
            boolean requiredIndicator) {
        super(activity, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        // Sets transparent background for animating content view.
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mActivity = activity;
        mHandler = new Handler();
        mIsDismissed = false;
        mEditorActionListener = new TextView.OnEditorActionListener() {
            @Override
            @SuppressWarnings("WrongConstant") // https://crbug.com/1038784
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    mDoneButton.performClick();
                    return true;
                } else if (actionId == EditorInfo.IME_ACTION_NEXT) {
                    View next = v.focusSearch(View.FOCUS_FORWARD);
                    if (next != null) {
                        next.requestFocus();
                        return true;
                    }
                }
                return false;
            }
        };

        mHalfRowMargin = activity.getResources().getDimensionPixelSize(
                R.dimen.editor_dialog_section_large_spacing);
        mFieldViews = new ArrayList<>();
        mEditableTextFields = new ArrayList<>();
        mDropdownFields = new ArrayList<>();

        final Pattern cardNumberPattern = Pattern.compile("^[\\d- ]*$");
        mCardNumberInputFilter = new InputFilter() {
            @Override
            public CharSequence filter(
                    CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                // Accept deletions.
                if (start == end) return null;

                // Accept digits, "-", and spaces.
                if (cardNumberPattern.matcher(source.subSequence(start, end)).matches()) {
                    return null;
                }

                // Reject everything else.
                return "";
            }
        };

        mCardNumberFormatter = new CreditCardNumberFormattingTextWatcher();
        mDeleteRunnable = deleteRunnable;
        mProfile = profile;
        mHasRequiredIndicator = requiredIndicator;
    }

    /**
     * @return The browser profile that is associated with the content being edited.
     */
    public Profile getProfile() {
        return mProfile;
    }

    /** Prevents screenshots of this editor. */
    public void disableScreenshots() {
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
        getWindow().setAttributes(attributes);
    }

    /**
     * @param shouldTrigger If true, done callback is triggered immediately after the user clicked
     *         on the done button. Otherwise, by default, it is triggered only after the dialog is
     *         dismissed with animation.
     */
    public void setShouldTriggerDoneCallbackBeforeCloseAnimation(boolean shouldTrigger) {
        mShouldTriggerDoneCallbackBeforeCloseAnimation = shouldTrigger;
    }

    /**
     * Prepares the toolbar for use.
     *
     * Many of the things that would ideally be set as attributes don't work and need to be set
     * programmatically.  This is likely due to how we compile the support libraries.
     */
    private void prepareToolbar() {
        EditorDialogToolbar toolbar = (EditorDialogToolbar) mLayout.findViewById(R.id.action_bar);
        toolbar.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(toolbar.getContext()));
        toolbar.setTitleTextAppearance(
                toolbar.getContext(), R.style.TextAppearance_Headline_Primary);
        toolbar.setTitle(mEditorModel.getTitle());
        toolbar.setShowDeleteMenuItem(mDeleteRunnable != null);

        // Show the help article when the help icon is clicked on, or delete
        // the profile and go back when the delete icon is clicked on.
        toolbar.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.delete_menu_id) {
                    if (mEditorModel.getDeleteConfirmationTitle() != null
                            || mEditorModel.getDeleteConfirmationText() != null) {
                        handleDeleteWithConfirmation(mEditorModel.getDeleteConfirmationTitle(),
                                mEditorModel.getDeleteConfirmationText());
                    } else {
                        handleDelete();
                    }
                } else if (item.getItemId() == R.id.help_menu_id) {
                    HelpAndFeedbackLauncherImpl.getForProfile(mProfile).show(
                            mActivity, mActivity.getString(R.string.help_context_autofill), null);
                }
                return true;
            }
        });

        // Cancel editing when the user hits the back arrow.
        toolbar.setNavigationContentDescription(R.string.cancel);
        toolbar.setNavigationIcon(getTintedBackIcon());
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                animateOutDialog();
            }
        });

        // The top shadow is handled by the toolbar, so hide the one used in the field editor.
        FadingEdgeScrollView scrollView =
                (FadingEdgeScrollView) mLayout.findViewById(R.id.scroll_view);
        scrollView.setEdgeVisibility(EdgeType.NONE, EdgeType.FADING);

        // The shadow's top margin doesn't get picked up in the xml; set it programmatically.
        View shadow = mLayout.findViewById(R.id.shadow);
        LayoutParams params = (LayoutParams) shadow.getLayoutParams();
        params.topMargin = toolbar.getLayoutParams().height;
        shadow.setLayoutParams(params);
        scrollView.getViewTreeObserver().addOnScrollChangedListener(
                SettingsUtils.getShowShadowOnScrollListener(scrollView, shadow));
    }

    /**
     * Checks if all of the fields in the form are valid and updates the displayed errors. If there
     * are any invalid fields, makes sure that one of them is focused. Called when user taps [SAVE].
     *
     * @return Whether all fields contain valid information.
     */
    public boolean validateForm() {
        final List<EditorFieldView> invalidViews = getViewsWithInvalidInformation(true);

        // Iterate over all the fields to update what errors are displayed, which is necessary to
        // to clear existing errors on any newly valid fields.
        for (int i = 0; i < mFieldViews.size(); i++) {
            EditorFieldView fieldView = mFieldViews.get(i);
            fieldView.updateDisplayedError(invalidViews.contains(fieldView));
        }

        if (!invalidViews.isEmpty()) {
            // Make sure that focus is on an invalid field.
            EditorFieldView focusedField = getEditorTextField(getCurrentFocus());
            if (invalidViews.contains(focusedField)) {
                // The focused field is invalid, but it may be scrolled off screen. Scroll to it.
                focusedField.scrollToAndFocus();
            } else {
                // Some fields are invalid, but none of the are focused. Scroll to the first invalid
                // field and focus it.
                invalidViews.get(0).scrollToAndFocus();
            }
        }

        if (!invalidViews.isEmpty() && sObserverForTest != null) {
            sObserverForTest.onEditorValidationError();
        }

        return invalidViews.isEmpty();
    }

    /** @return The validatable item for the given view. */
    private EditorFieldView getEditorTextField(View v) {
        if (v instanceof TextView && v.getParent() != null
                && v.getParent() instanceof EditorFieldView) {
            return (EditorFieldView) v.getParent();
        } else if (v instanceof Spinner && v.getTag() != null) {
            return (EditorFieldView) v.getTag();
        } else {
            return null;
        }
    }

    @Override
    public void onClick(View view) {
        // Disable interaction during animation.
        if (mDialogInOutAnimator != null) return;

        if (view.getId() == R.id.editor_dialog_done_button) {
            if (validateForm()) {
                if (mShouldTriggerDoneCallbackBeforeCloseAnimation && mEditorModel != null) {
                    mEditorModel.done();
                    mEditorModel = null;
                }
                mFormWasValid = true;
                animateOutDialog();
                return;
            }
        } else if (view.getId() == R.id.payments_edit_cancel_button) {
            animateOutDialog();
        }
    }

    private void animateOutDialog() {
        if (mDialogInOutAnimator != null || !isShowing()) return;

        if (getCurrentFocus() != null) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(getCurrentFocus());
        }

        Animator dropDown =
                ObjectAnimator.ofFloat(mLayout, View.TRANSLATION_Y, 0f, mLayout.getHeight());
        Animator fadeOut = ObjectAnimator.ofFloat(mLayout, View.ALPHA, mLayout.getAlpha(), 0f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(dropDown, fadeOut);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_EXIT_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mDialogInOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mDialogInOutAnimator = null;
                dismiss();
            }
        });

        mDialogInOutAnimator.start();
    }

    public void setAsNotDismissed() {
        mIsDismissed = false;
    }

    public boolean isDismissed() {
        return mIsDismissed;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mIsDismissed = true;
        if (mEditorModel != null) {
            if (mFormWasValid) {
                mEditorModel.done();
                mFormWasValid = false;
            } else {
                mEditorModel.cancel();
            }
            mEditorModel = null;
        }
        removeTextChangedListenersAndInputFilters();
    }

    private void prepareButtons() {
        mDoneButton = (Button) mLayout.findViewById(R.id.button_primary);
        mDoneButton.setId(R.id.editor_dialog_done_button);
        mDoneButton.setOnClickListener(this);
        if (mEditorModel.getCustomDoneButtonText() != null) {
            mDoneButton.setText(mEditorModel.getCustomDoneButtonText());
        }

        Button cancelButton = (Button) mLayout.findViewById(R.id.button_secondary);
        cancelButton.setId(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);
    }

    private void prepareFooter() {
        assert mEditorModel != null;

        TextView requiredFieldsNotice = mLayout.findViewById(R.id.required_fields_notice);
        int requiredFieldsNoticeVisibility = View.GONE;
        if (mHasRequiredIndicator) {
            for (int i = 0; i < mFieldViews.size(); i++) {
                if (mFieldViews.get(i).isRequired()) {
                    requiredFieldsNoticeVisibility = View.VISIBLE;
                    break;
                }
            }
        }
        requiredFieldsNotice.setVisibility(requiredFieldsNoticeVisibility);

        TextView footerMessage = mLayout.findViewById(R.id.footer_message);
        String footerMessageText = mEditorModel.getFooterMessageText();
        if (footerMessageText != null) {
            footerMessage.setText(footerMessageText);
            footerMessage.setVisibility(View.VISIBLE);
        } else {
            footerMessage.setVisibility(View.GONE);
        }
    }

    /**
     * Create the visual representation of the EditorModel.
     *
     * This would be more optimal as a RelativeLayout, but because it's dynamically generated, it's
     * much more human-parsable with inefficient LinearLayouts for half-width controls sharing rows.
     */
    private void prepareEditor() {
        assert mEditorModel != null;

        // Ensure the layout is empty.
        removeTextChangedListenersAndInputFilters();
        mDataView = (ViewGroup) mLayout.findViewById(R.id.contents);
        mDataView.removeAllViews();
        mFieldViews.clear();
        mEditableTextFields.clear();
        mDropdownFields.clear();

        // Add Views for each of the {@link EditorFields}.
        for (int i = 0; i < mEditorModel.getFields().size(); i++) {
            EditorFieldModel fieldModel = mEditorModel.getFields().get(i);
            EditorFieldModel nextFieldModel = null;

            boolean isLastField = i == mEditorModel.getFields().size() - 1;
            boolean useFullLine = fieldModel.isFullLine();
            if (!isLastField && !useFullLine) {
                // If the next field isn't full, stretch it out.
                nextFieldModel = mEditorModel.getFields().get(i + 1);
                if (nextFieldModel.isFullLine()) useFullLine = true;
            }

            // Always keep dropdowns and text fields on different lines because of height
            // differences.
            if (!isLastField && !useFullLine
                    && fieldModel.isDropdownField() != nextFieldModel.isDropdownField()) {
                useFullLine = true;
            }

            if (useFullLine || isLastField) {
                addFieldViewToEditor(mDataView, fieldModel);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(mActivity);
                mDataView.addView(rowLayout);

                View firstView = addFieldViewToEditor(rowLayout, fieldModel);
                View lastView = addFieldViewToEditor(rowLayout, nextFieldModel);

                LinearLayout.LayoutParams firstParams =
                        (LinearLayout.LayoutParams) firstView.getLayoutParams();
                LinearLayout.LayoutParams lastParams =
                        (LinearLayout.LayoutParams) lastView.getLayoutParams();

                firstParams.width = 0;
                firstParams.weight = 1;
                MarginLayoutParamsCompat.setMarginEnd(firstParams, mHalfRowMargin);
                lastParams.width = 0;
                lastParams.weight = 1;

                i = i + 1;
            }
        }

        // Add the footer.
        mDataView.addView(mFooter);
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered
     * by adding padding to both sides.
     */
    public void onConfigurationChanged() {
        if (mUiConfig == null) {
            if (mDataView != null) {
                int minWidePaddingPixels = mActivity.getResources().getDimensionPixelSize(
                        R.dimen.settings_wide_display_min_padding);
                mUiConfig = new UiConfig(mDataView);
                ViewResizer.createAndAttach(mDataView, mUiConfig, 0, minWidePaddingPixels);
            }
        } else {
            mUiConfig.updateDisplayStyle();
        }
    }

    private void removeTextChangedListenersAndInputFilters() {
        if (mCardInput != null) {
            mCardInput.removeTextChangedListener(mCardNumberFormatter);
            mCardInput.setFilters(new InputFilter[0]); // Null is not allowed.
            mCardInput = null;
        }

        if (mPhoneInput != null) {
            mPhoneInput.removeTextChangedListener(mPhoneFormatter);
            mPhoneInput = null;
        }
    }

    private View addFieldViewToEditor(ViewGroup parent, final EditorFieldModel fieldModel) {
        View childView = null;

        if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_ICONS) {
            childView = new EditorIconsField(mActivity, parent, fieldModel).getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_LABEL) {
            childView = new EditorLabelField(mActivity, parent, fieldModel).getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN) {
            Runnable prepareEditorRunnable = new Runnable() {
                @Override
                public void run() {
                    // The dialog has been dismissed.
                    if (mEditorModel == null) return;

                    // The fields may have changed.
                    prepareEditor();
                    prepareFooter();
                    if (sObserverForTest != null) sObserverForTest.onEditorReadyToEdit();
                }
            };
            EditorDropdownField dropdownView = new EditorDropdownField(
                    mActivity, parent, fieldModel, prepareEditorRunnable, mHasRequiredIndicator);
            mFieldViews.add(dropdownView);
            mDropdownFields.add(dropdownView.getDropdown());

            childView = dropdownView.getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_CHECKBOX) {
            final CheckBox checkbox = new CheckBox(mLayout.getContext());
            checkbox.setId(R.id.payments_edit_checkbox);
            checkbox.setText(fieldModel.getLabel());
            checkbox.setChecked(fieldModel.isChecked());
            checkbox.setMinimumHeight(mActivity.getResources().getDimensionPixelSize(
                    R.dimen.editor_dialog_checkbox_min_height));
            checkbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                    fieldModel.setIsChecked(isChecked);
                    if (sObserverForTest != null) sObserverForTest.onEditorReadyToEdit();
                }
            });

            childView = checkbox;
        } else {
            InputFilter filter = null;
            TextWatcher formatter = null;
            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_CREDIT_CARD) {
                filter = mCardNumberInputFilter;
                formatter = mCardNumberFormatter;
            } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                mPhoneFormatter = fieldModel.getFormatter();
                assert mPhoneFormatter != null;
                formatter = mPhoneFormatter;
            }

            EditorTextField inputLayout =
                    new EditorTextField(mActivity, fieldModel, mEditorActionListener, filter,
                            formatter, /* focusAndShowKeyboard= */ false, mHasRequiredIndicator);
            mFieldViews.add(inputLayout);

            EditText input = inputLayout.getEditText();
            mEditableTextFields.add(input);

            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_CREDIT_CARD) {
                assert mCardInput == null;
                mCardInput = input;
            } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                assert mPhoneInput == null;
                mPhoneInput = input;
            }

            childView = inputLayout;
        }

        parent.addView(childView);
        return childView;
    }

    /**
     * Displays the editor user interface for the given model.
     *
     * @param editorModel The description of the editor user interface to display.
     */
    public void show(EditorModel editorModel) {
        // If an asynchronous task calls show, while the activity is already finishing, return.
        if (mActivity.isFinishing()) return;

        setOnShowListener(this);
        setOnDismissListener(this);
        mEditorModel = editorModel;
        mLayout = LayoutInflater.from(mActivity).inflate(R.layout.payment_request_editor, null);
        setContentView(mLayout);

        mFooter = LayoutInflater.from(mActivity).inflate(
                R.layout.editable_option_editor_footer, null, false);

        prepareToolbar();
        prepareEditor();
        prepareFooter();
        prepareButtons();
        onConfigurationChanged();

        // Temporarily hide the content to avoid blink before animation starts.
        mLayout.setVisibility(View.INVISIBLE);
        show();
    }

    /** Rereads the values in the model to update the UI. */
    public void update() {
        for (int i = 0; i < mFieldViews.size(); i++) {
            mFieldViews.get(i).update();
        }
    }

    @Override
    public void onShow(DialogInterface dialog) {
        if (mDialogInOutAnimator != null && mIsDismissed) return;

        // Hide keyboard and disable EditText views for animation efficiency.
        if (getCurrentFocus() != null) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(getCurrentFocus());
        }
        for (int i = 0; i < mEditableTextFields.size(); i++) {
            mEditableTextFields.get(i).setEnabled(false);
        }

        mLayout.setVisibility(View.VISIBLE);
        mLayout.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mLayout.buildLayer();
        Animator popUp =
                ObjectAnimator.ofFloat(mLayout, View.TRANSLATION_Y, mLayout.getHeight(), 0f);
        Animator fadeIn = ObjectAnimator.ofFloat(mLayout, View.ALPHA, 0f, 1f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(popUp, fadeIn);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mDialogInOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mLayout.setLayerType(View.LAYER_TYPE_NONE, null);
                for (int i = 0; i < mEditableTextFields.size(); i++) {
                    mEditableTextFields.get(i).setEnabled(true);
                }
                mDialogInOutAnimator = null;
                initFocus();
            }
        });

        mDialogInOutAnimator.start();
    }

    private void initFocus() {
        mHandler.post(() -> {
            // If TalkBack is enabled, we want to keep the focus at the top
            // because the user would not learn about the elements that are
            // above the focused field.
            if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                List<EditorFieldView> invalidViews = getViewsWithInvalidInformation(false);
                if (!invalidViews.isEmpty()) {
                    // Immediately focus the first invalid field to make it faster to edit.
                    invalidViews.get(0).scrollToAndFocus();
                } else {
                    // Trigger default focus as it is not triggered automatically on Android P+.
                    mLayout.requestFocus();
                }
            }
            // Note that keyboard will not be shown for dropdown field since it's not necessary.
            if (getCurrentFocus() != null) {
                KeyboardVisibilityDelegate.getInstance().showKeyboard(getCurrentFocus());
                // Put the cursor to the end of the text.
                if (getCurrentFocus() instanceof EditText) {
                    EditText focusedEditText = (EditText) getCurrentFocus();
                    focusedEditText.setSelection(focusedEditText.getText().length());
                }
            }
            if (sObserverForTest != null) sObserverForTest.onEditorReadyToEdit();
        });
    }

    private void handleDelete() {
        mDeleteRunnable.run();
        animateOutDialog();
    }

    private void handleDeleteWithConfirmation(
            @Nullable String confirmationTitle, @Nullable String confirmationText) {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View body = inflater.inflate(R.layout.confirmation_dialog_view, null);
        TextView titleView = body.findViewById(R.id.confirmation_dialog_title);
        titleView.setText(confirmationTitle);
        TextView messageView = body.findViewById(R.id.confirmation_dialog_message);
        messageView.setText(confirmationText);

        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(body)
                        .setNegativeButton(R.string.cancel,
                                (dialog, which) -> {
                                    dialog.cancel();
                                    mConfirmationDialog = null;
                                    if (sObserverForTest != null) {
                                        sObserverForTest.onEditorReadyToEdit();
                                    }
                                })
                        .setPositiveButton(R.string.delete,
                                (dialog, which) -> {
                                    handleDelete();
                                    mConfirmationDialog = null;
                                })
                        .create();
        mConfirmationDialog.show();

        if (sObserverForTest != null) {
            sObserverForTest.onEditorConfirmationDialogShown();
        }
    }

    private List<EditorFieldView> getViewsWithInvalidInformation(boolean findAll) {
        List<EditorFieldView> invalidViews = new ArrayList<>();
        for (int i = 0; i < mFieldViews.size(); i++) {
            EditorFieldView fieldView = mFieldViews.get(i);
            if (!fieldView.isValid()) {
                invalidViews.add(fieldView);
                if (!findAll) break;
            }
        }
        return invalidViews;
    }

    /** @return The View with all fields of this editor. */
    @VisibleForTesting
    public View getDataViewForTest() {
        return mDataView;
    }

    /** @return All editable text fields in the editor. Used only for tests. */
    @VisibleForTesting
    public List<EditText> getEditableTextFieldsForTest() {
        return mEditableTextFields;
    }

    /** @return All dropdown fields in the editor. Used only for tests. */
    @VisibleForTesting
    public List<Spinner> getDropdownFieldsForTest() {
        return mDropdownFields;
    }

    @VisibleForTesting
    public AlertDialog getConfirmationDialogForTest() {
        return mConfirmationDialog;
    }

    @VisibleForTesting
    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        EditorDropdownField.setEditorObserverForTest(sObserverForTest);
        EditorTextField.setEditorObserverForTest(sObserverForTest);
    }

    private Drawable getTintedBackIcon() {
        return TintedDrawable.constructTintedDrawable(getContext(),
                R.drawable.ic_arrow_back_white_24dp, R.color.default_icon_color_tint_list);
    }
}
