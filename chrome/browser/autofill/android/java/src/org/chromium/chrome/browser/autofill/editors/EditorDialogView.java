// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FOOTER_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_FULL_LINE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.isDropdownField;

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
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
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

import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.AlwaysDismissedDialog;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView.EdgeType;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * The editor dialog. Can be used for editing contact information, shipping address,
 * billing address.
 *
 * TODO(https://crbug.com/799905): Move payment specific functionality to separate class.
 */
public class EditorDialogView
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
    private final HelpAndFeedbackLauncher mHelpLauncher;
    private final Handler mHandler;
    private final TextView.OnEditorActionListener mEditorActionListener;
    private final int mHalfRowMargin;
    private final List<FieldView> mFieldViews;
    private final List<EditText> mEditableTextFields;
    private final List<Spinner> mDropdownFields;

    private View mLayout;
    private PropertyModel mEditorModel;
    private Button mDoneButton;
    private boolean mFormWasValid;
    private ViewGroup mDataView;
    private View mFooter;

    private Animator mDialogInOutAnimator;
    private boolean mIsDismissed;
    @Nullable
    private UiConfig mUiConfig;
    @Nullable
    private AlertDialog mConfirmationDialog;

    /**
     * Builds the editor dialog.
     *
     * @param activity             The activity on top of which the UI should be displayed.
     * @param helpLauncher         The launcher of user help activity.
     */
    public EditorDialogView(Activity activity, HelpAndFeedbackLauncher helpLauncher) {
        super(activity, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        // Sets transparent background for animating content view.
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mActivity = activity;
        mHelpLauncher = helpLauncher;
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
    }

    /** Prevents screenshots of this editor. */
    public void disableScreenshots() {
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
        getWindow().setAttributes(attributes);
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
        toolbar.setTitle(mEditorModel.get(EDITOR_TITLE));
        toolbar.setShowDeleteMenuItem(mEditorModel.get(ALLOW_DELETE));

        // Show the help article when the help icon is clicked on, or delete
        // the profile and go back when the delete icon is clicked on.
        toolbar.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.delete_menu_id) {
                    if (mEditorModel.get(DELETE_CONFIRMATION_TITLE) != null
                            || mEditorModel.get(DELETE_CONFIRMATION_TEXT) != null) {
                        handleDeleteWithConfirmation(mEditorModel.get(DELETE_CONFIRMATION_TITLE),
                                mEditorModel.get(DELETE_CONFIRMATION_TEXT));
                    } else {
                        handleDelete();
                    }
                } else if (item.getItemId() == R.id.help_menu_id) {
                    mHelpLauncher.show(
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
        final List<FieldView> invalidViews = getViewsWithInvalidInformation(true);

        // Iterate over all the fields to update what errors are displayed, which is necessary to
        // to clear existing errors on any newly valid fields.
        for (int i = 0; i < mFieldViews.size(); i++) {
            FieldView fieldView = mFieldViews.get(i);
            fieldView.updateDisplayedError(invalidViews.contains(fieldView));
        }

        if (!invalidViews.isEmpty()) {
            // Make sure that focus is on an invalid field.
            FieldView focusedField = getTextFieldView(getCurrentFocus());
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
    private FieldView getTextFieldView(View v) {
        if (v instanceof TextView && v.getParent() != null && v.getParent() instanceof FieldView) {
            return (FieldView) v.getParent();
        } else if (v instanceof Spinner && v.getTag() != null) {
            return (FieldView) v.getTag();
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
                if (mEditorModel.get(TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION)
                        && mEditorModel != null) {
                    mEditorModel.get(DONE_RUNNABLE).run();
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

    public void setEditorFields(ListModel<ListItem> editorFields) {
        // The dialog has been dismissed.
        if (mEditorModel == null) return;

        prepareEditor(editorFields);
        prepareFooter();

        if (sObserverForTest != null) sObserverForTest.onEditorReadyToEdit();
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
                mEditorModel.get(DONE_RUNNABLE).run();
                mFormWasValid = false;
            } else {
                mEditorModel.get(CANCEL_RUNNABLE).run();
            }
            mEditorModel = null;
        }
        removeTextChangedListeners();
    }

    private void prepareButtons() {
        mDoneButton = (Button) mLayout.findViewById(R.id.button_primary);
        mDoneButton.setId(R.id.editor_dialog_done_button);
        mDoneButton.setOnClickListener(this);
        if (mEditorModel.get(CUSTOM_DONE_BUTTON_TEXT) != null) {
            mDoneButton.setText(mEditorModel.get(CUSTOM_DONE_BUTTON_TEXT));
        }

        Button cancelButton = (Button) mLayout.findViewById(R.id.button_secondary);
        cancelButton.setId(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);
    }

    private void prepareFooter() {
        assert mEditorModel != null;

        TextView requiredFieldsNotice = mLayout.findViewById(R.id.required_fields_notice);
        int requiredFieldsNoticeVisibility = View.GONE;
        if (mEditorModel.get(EditorProperties.SHOW_REQUIRED_INDICATOR)) {
            for (int i = 0; i < mFieldViews.size(); i++) {
                if (mFieldViews.get(i).isRequired()) {
                    requiredFieldsNoticeVisibility = View.VISIBLE;
                    break;
                }
            }
        }
        requiredFieldsNotice.setVisibility(requiredFieldsNoticeVisibility);

        TextView footerMessage = mLayout.findViewById(R.id.footer_message);
        String footerMessageText = mEditorModel.get(FOOTER_MESSAGE);
        if (footerMessageText != null) {
            footerMessage.setText(footerMessageText);
            footerMessage.setVisibility(View.VISIBLE);
        } else {
            footerMessage.setVisibility(View.GONE);
        }
    }

    /**
     * Create the visual representation of the PropertyModel defined by {@link EditorProperties}.
     *
     * This would be more optimal as a RelativeLayout, but because it's dynamically generated, it's
     * much more human-parsable with inefficient LinearLayouts for half-width controls sharing rows.
     *
     * @param editorFields the list of fields this editor should display.
     */
    private void prepareEditor(ListModel<ListItem> editorFields) {
        assert mEditorModel != null;

        // Ensure the layout is empty.
        removeTextChangedListeners();
        mDataView = (ViewGroup) mLayout.findViewById(R.id.contents);
        mDataView.removeAllViews();
        mFieldViews.clear();
        mEditableTextFields.clear();
        mDropdownFields.clear();

        // Add Views for each of the {@link EditorFields}.
        ListModel<ListItem> fields = mEditorModel.get(EDITOR_FIELDS);
        for (int i = 0; i < fields.size(); i++) {
            ListItem fieldItem = fields.get(i);
            ListItem nextFieldItem = null;

            boolean isLastField = i == fields.size() - 1;
            boolean useFullLine = fieldItem.model.get(IS_FULL_LINE);
            if (!isLastField && !useFullLine) {
                // If the next field isn't full, stretch it out.
                nextFieldItem = fields.get(i + 1);
                if (nextFieldItem.model.get(IS_FULL_LINE)) useFullLine = true;
            }

            // Always keep dropdowns and text fields on different lines because of height
            // differences.
            if (!isLastField && !useFullLine
                    && isDropdownField(fieldItem) != isDropdownField(nextFieldItem)) {
                useFullLine = true;
            }

            if (useFullLine || isLastField) {
                addFieldViewToEditor(mDataView, fieldItem);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(mActivity);
                mDataView.addView(rowLayout);

                View firstView = addFieldViewToEditor(rowLayout, fieldItem);
                View lastView = addFieldViewToEditor(rowLayout, nextFieldItem);

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

    private void removeTextChangedListeners() {
        for (FieldView view : mFieldViews) {
            if (view instanceof TextFieldView) {
                TextFieldView textView = (TextFieldView) view;
                textView.removeTextChangedListeners();
            }
        }
    }

    private View addFieldViewToEditor(ViewGroup parent, final ListItem fieldItem) {
        View childView = null;

        switch (fieldItem.type) {
            case DROPDOWN: {
                DropdownFieldView dropdownView =
                        new DropdownFieldView(mActivity, parent, fieldItem.model,
                                mEditorModel.get(EditorProperties.SHOW_REQUIRED_INDICATOR));
                mFieldViews.add(dropdownView);
                mDropdownFields.add(dropdownView.getDropdown());

                childView = dropdownView.getLayout();
                break;
            }
            case TEXT_INPUT: {
                TextFieldView inputLayout = new TextFieldView(mActivity, fieldItem.model,
                        mEditorActionListener, fieldItem.model.get(TEXT_FORMATTER),
                        mEditorModel.get(EditorProperties.SHOW_REQUIRED_INDICATOR));
                mFieldViews.add(inputLayout);

                mEditableTextFields.add(inputLayout.getEditText());
                childView = inputLayout;
                break;
            }
        }
        parent.addView(childView);
        return childView;
    }

    /**
     * Displays the editor user interface for the given model.
     *
     * @param editorModel The description of the editor user interface to display.
     */
    public void show(PropertyModel editorModel) {
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
        prepareEditor(editorModel.get(EDITOR_FIELDS));
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
                List<FieldView> invalidViews = getViewsWithInvalidInformation(false);
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
        assert mEditorModel.get(DELETE_RUNNABLE) != null;
        mEditorModel.get(DELETE_RUNNABLE).run();
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

    private List<FieldView> getViewsWithInvalidInformation(boolean findAll) {
        List<FieldView> invalidViews = new ArrayList<>();
        for (int i = 0; i < mFieldViews.size(); i++) {
            FieldView fieldView = mFieldViews.get(i);
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
        DropdownFieldView.setEditorObserverForTest(sObserverForTest);
        TextFieldView.setEditorObserverForTest(sObserverForTest);
    }

    private Drawable getTintedBackIcon() {
        return TintedDrawable.constructTintedDrawable(getContext(),
                R.drawable.ic_arrow_back_white_24dp, R.color.default_icon_color_tint_list);
    }
}
