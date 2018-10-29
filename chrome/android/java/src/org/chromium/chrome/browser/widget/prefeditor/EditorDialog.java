// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.prefeditor;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v4.view.animation.LinearOutSlowInInterpolator;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
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
import android.widget.RelativeLayout;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.autofill.CreditCardNumberFormattingTextWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.chrome.browser.widget.TintedDrawable;
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

    private final Context mContext;
    private final EditorObserverForTest mObserverForTest;
    private final Handler mHandler;
    private final TextView.OnEditorActionListener mEditorActionListener;
    private final int mHalfRowMargin;
    private final int mDropdownTopPadding;
    private final List<EditorFieldView> mFieldViews;
    private final List<EditText> mEditableTextFields;
    private final List<Spinner> mDropdownFields;
    private final InputFilter mCardNumberInputFilter;
    private final TextWatcher mCardNumberFormatter;

    @Nullable
    private TextWatcher mPhoneFormatter;
    private View mLayout;
    private EditorModel mEditorModel;
    private Button mDoneButton;
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
    /**
     * Builds the editor dialog.
     *
     * @param activity        The activity on top of which the UI should be displayed.
     * @param observerForTest Optional event observer for testing.
     * @param deleteRunnable  The runnable that when called will delete the profile.
     */
    public EditorDialog(
            Activity activity, EditorObserverForTest observerForTest, Runnable deleteRunnable) {
        super(activity, R.style.FullscreenWhite);
        // Sets transparent background for animating content view.
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mContext = activity;
        mObserverForTest = observerForTest;
        mHandler = new Handler();
        mIsDismissed = false;
        mEditorActionListener = new TextView.OnEditorActionListener() {
            @Override
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
        mDropdownTopPadding = activity.getResources().getDimensionPixelSize(
                R.dimen.payments_section_dropdown_top_padding);
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
    }

    /** Prevents screenshots of this editor. */
    public void disableScreenshots() {
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
        getWindow().setAttributes(attributes);
    }

    /** Launches the Autofill help page on top of the current Context. */
    public static void launchAutofillHelpPage(Context context) {
        HelpAndFeedback.getInstance(context).show((Activity) context,
                context.getString(R.string.help_context_autofill), Profile.getLastUsedProfile(),
                null);
    }

    /**
     * Prepares the toolbar for use.
     *
     * Many of the things that would ideally be set as attributes don't work and need to be set
     * programmatically.  This is likely due to how we compile the support libraries.
     */
    private void prepareToolbar() {
        EditorDialogToolbar toolbar = (EditorDialogToolbar) mLayout.findViewById(R.id.action_bar);
        toolbar.setBackgroundColor(ApiCompatibilityUtils.getColor(
                toolbar.getResources(), R.color.modern_primary_color));
        toolbar.setTitleTextAppearance(toolbar.getContext(), R.style.BlackHeadline);
        toolbar.setTitle(mEditorModel.getTitle());
        toolbar.setShowDeleteMenuItem(mDeleteRunnable != null);

        // Show the help article when the help icon is clicked on, or delete
        // the profile and go back when the delete icon is clicked on.
        toolbar.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.delete_menu_id) {
                    mDeleteRunnable.run();
                    animateOutDialog();
                } else if (item.getItemId() == R.id.help_menu_id) {
                    launchAutofillHelpPage(mContext);
                }
                return true;
            }
        });

        // Cancel editing when the user hits the back arrow.
        toolbar.setNavigationContentDescription(R.string.cancel);
        toolbar.setNavigationIcon(getBlackTintedBackIcon());
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                animateOutDialog();
            }
        });

        // The top shadow is handled by the toolbar, so hide the one used in the field editor.
        FadingEdgeScrollView scrollView =
                (FadingEdgeScrollView) mLayout.findViewById(R.id.scroll_view);
        scrollView.setEdgeVisibility(
                FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.FADING);

        // The shadow's top margin doesn't get picked up in the xml; set it programmatically.
        View shadow = mLayout.findViewById(R.id.shadow);
        RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) shadow.getLayoutParams();
        params.topMargin = toolbar.getLayoutParams().height;
        shadow.setLayoutParams(params);
        scrollView.getViewTreeObserver().addOnScrollChangedListener(
                PreferenceUtils.getShowShadowOnScrollListener(scrollView, shadow));
    }

    /**
     * Checks if all of the fields in the form are valid and updates the displayed errors. If there
     * are any invalid fields, makes sure that one of them is focused. Called when user taps [SAVE].
     *
     * @return Whether all fields contain valid information.
     */
    private boolean validateForm() {
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
                if (mEditorModel != null) mEditorModel.done();
                mEditorModel = null;
                animateOutDialog();
                return;
            }

            if (mObserverForTest != null) mObserverForTest.onEditorValidationError();
        } else if (view.getId() == R.id.payments_edit_cancel_button) {
            animateOutDialog();
        }
    }

    private void animateOutDialog() {
        if (mDialogInOutAnimator != null || !isShowing()) return;

        Animator dropDown =
                ObjectAnimator.ofFloat(mLayout, View.TRANSLATION_Y, 0f, mLayout.getHeight());
        Animator fadeOut = ObjectAnimator.ofFloat(mLayout, View.ALPHA, mLayout.getAlpha(), 0f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(dropDown, fadeOut);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_EXIT_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(new FastOutLinearInInterpolator());
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
        if (mEditorModel != null) mEditorModel.cancel();
        removeTextChangedListenersAndInputFilters();
    }

    private void prepareButtons() {
        mDoneButton = (Button) mLayout.findViewById(R.id.button_primary);
        mDoneButton.setId(R.id.editor_dialog_done_button);
        mDoneButton.setOnClickListener(this);

        Button cancelButton = (Button) mLayout.findViewById(R.id.button_secondary);
        cancelButton.setId(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);
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

            if (useFullLine || isLastField) {
                addFieldViewToEditor(mDataView, fieldModel);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(mContext);
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

                // Align the text field and the dropdown field.
                if ((fieldModel.isTextField() && nextFieldModel.isDropdownField())
                        || (nextFieldModel.isTextField() && fieldModel.isDropdownField())) {
                    LinearLayout.LayoutParams dropdownParams =
                            fieldModel.isDropdownField() ? firstParams : lastParams;
                    dropdownParams.topMargin = mDropdownTopPadding;
                    dropdownParams.bottomMargin = 0;
                }

                i = i + 1;
            }
        }

        // Add the footer.
        mDataView.addView(mFooter);
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
            childView = new EditorIconsField(mContext, parent, fieldModel).getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_LABEL) {
            childView = new EditorLabelField(mContext, parent, fieldModel).getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN) {
            Runnable prepareEditorRunnable = new Runnable() {
                @Override
                public void run() {
                    // The dialog has been dismissed.
                    if (mEditorModel == null) return;

                    // The fields may have changed.
                    prepareEditor();
                    if (mObserverForTest != null) mObserverForTest.onEditorReadyToEdit();
                }
            };
            EditorDropdownField dropdownView = new EditorDropdownField(
                    mContext, parent, fieldModel, prepareEditorRunnable, mObserverForTest);
            mFieldViews.add(dropdownView);
            mDropdownFields.add(dropdownView.getDropdown());

            childView = dropdownView.getLayout();
        } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_CHECKBOX) {
            final CheckBox checkbox = new CheckBox(mLayout.getContext());
            checkbox.setId(R.id.payments_edit_checkbox);
            checkbox.setText(fieldModel.getLabel());
            checkbox.setChecked(fieldModel.isChecked());
            checkbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                    fieldModel.setIsChecked(isChecked);
                    if (mObserverForTest != null) mObserverForTest.onEditorReadyToEdit();
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

            EditorTextField inputLayout = new EditorTextField(mContext, fieldModel,
                    mEditorActionListener, filter, formatter, mObserverForTest);
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
        if (((Activity) mContext).isFinishing()) return;

        setOnShowListener(this);
        setOnDismissListener(this);
        mEditorModel = editorModel;
        mLayout = LayoutInflater.from(mContext).inflate(R.layout.payment_request_editor, null);
        setContentView(mLayout);

        mFooter = LayoutInflater.from(mContext).inflate(
                R.layout.editable_option_editor_footer, null, false);

        prepareToolbar();
        prepareEditor();
        prepareButtons();
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

        mLayout.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mLayout.buildLayer();
        Animator popUp =
                ObjectAnimator.ofFloat(mLayout, View.TRANSLATION_Y, mLayout.getHeight(), 0f);
        Animator fadeIn = ObjectAnimator.ofFloat(mLayout, View.ALPHA, 0f, 1f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(popUp, fadeIn);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(new LinearOutSlowInInterpolator());
        mDialogInOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mLayout.setLayerType(View.LAYER_TYPE_NONE, null);
                for (int i = 0; i < mEditableTextFields.size(); i++) {
                    mEditableTextFields.get(i).setEnabled(true);
                }
                // Note that keyboard will not show for dropdown field since it's not necessary.
                if (getCurrentFocus() != null) {
                    KeyboardVisibilityDelegate.getInstance().showKeyboard(getCurrentFocus());
                }
                mDialogInOutAnimator = null;
                initFocus();
            }
        });

        mDialogInOutAnimator.start();
    }

    private void initFocus() {
        // Immediately focus the first invalid field to make it faster to edit.
        final List<EditorFieldView> invalidViews = getViewsWithInvalidInformation(false);
        if (!invalidViews.isEmpty()) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    invalidViews.get(0).scrollToAndFocus();
                    if (mObserverForTest != null) mObserverForTest.onEditorReadyToEdit();
                }
            });
        } else {
            // The first field will be focused, we are ready to edit.
            if (mObserverForTest != null) mObserverForTest.onEditorReadyToEdit();
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

    private Drawable getBlackTintedBackIcon() {
        return TintedDrawable.constructTintedDrawable(
                getContext(), R.drawable.ic_arrow_back_white_24dp, android.R.color.black);
    }
}
