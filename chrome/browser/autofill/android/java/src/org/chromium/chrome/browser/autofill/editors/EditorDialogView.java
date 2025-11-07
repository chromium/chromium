// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.NON_EDITABLE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.isDropdownField;

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
import android.text.method.LinkMovementMethod;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorItem;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
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
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * The editor dialog. Can be used for editing contact information, shipping address, billing
 * address.
 *
 * <p>TODO(crbug.com/41363594): Move payment specific functionality to separate class.
 */
@NullMarked
public class EditorDialogView extends AlwaysDismissedDialog
        implements OnClickListener,
                DialogInterface.OnShowListener,
                DialogInterface.OnDismissListener {
    /** The indicator for input fields that are required. */
    public static final String REQUIRED_FIELD_INDICATOR = "*";

    /** Duration of the animation to show the UI to full height. */
    private static final int DIALOG_ENTER_ANIMATION_MS = 300;

    /** Duration of the animation to hide the UI. */
    private static final int DIALOG_EXIT_ANIMATION_MS = 195;

    private @Nullable static EditorObserverForTest sObserverForTest;

    private final Activity mActivity;
    private final Context mContext;
    private final Profile mProfile;
    private final Handler mHandler;
    private final int mHalfRowMargin;
    private final List<FieldView> mFieldViews;
    // TODO(crbug.com/40265078): substitute this with SimpleRecyclerViewMCP.
    private final List<PropertyModelChangeProcessor<PropertyModel, TextFieldView, PropertyKey>>
            mTextFieldMCPs;
    private final List<PropertyModelChangeProcessor<PropertyModel, DropdownFieldView, PropertyKey>>
            mDropdownFieldMCPs;
    private final List<EditText> mEditableTextFields;
    private final List<Spinner> mDropdownFields;

    private final View mContainerView;
    private final ViewGroup mContentView;
    private final View mButtonBar;
    private Button mDoneButton;

    private @Nullable Animator mDialogInOutAnimator;
    private boolean mIsDismissed;
    private @Nullable UiConfig mUiConfig;
    private @Nullable AlertDialog mConfirmationDialog;

    private @Nullable String mDeleteConfirmationTitle;
    private @Nullable CharSequence mDeleteConfirmationText;
    private @Nullable String mDeleteConfirmationPrimaryButtonText;

    private @Nullable Runnable mDeleteRunnable;
    private @Nullable Runnable mDoneRunnable;
    private @Nullable Runnable mCancelRunnable;

    private @Nullable String mProfileRecordTypeSuffix;

    private boolean mValidateOnShow;

    @VisibleForTesting
    public static final String PROFILE_DELETED_HISTOGRAM = "Autofill.ProfileDeleted.Any.Total";

    @VisibleForTesting
    public static final String PROFILE_DELETED_SETTINGS_HISTOGRAM =
            "Autofill.ProfileDeleted.Settings.Total";

    /**
     * Builds the editor dialog.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param profile The Profile being edited.
     */
    public EditorDialogView(Activity activity, Profile profile) {
        super(
                activity,
                R.style.ThemeOverlay_BrowserUI_Fullscreen,
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
        // Sets transparent background for animating content view.
        assumeNonNull(getWindow()).setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mActivity = activity;
        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            // TODO(crbug.com/439911511): Set the style directly in the layout instead.
            mContext =
                    new ContextThemeWrapper(
                            activity, R.style.ThemeOverlay_Chromium_Settings_InputFields);
        } else {
            mContext = activity;
        }
        mProfile = profile;
        mHandler = new Handler();
        mIsDismissed = false;

        mHalfRowMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing);
        mFieldViews = new ArrayList<>();
        mTextFieldMCPs = new ArrayList<>();
        mDropdownFieldMCPs = new ArrayList<>();
        mEditableTextFields = new ArrayList<>();
        mDropdownFields = new ArrayList<>();

        setOnShowListener(this);
        setOnDismissListener(this);

        mContainerView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_editor_dialog, null);
        setContentView(mContainerView);

        prepareToolbar();

        mContentView = mContainerView.findViewById(R.id.contents);

        mButtonBar = mContainerView.findViewById(R.id.button_bar);
        mButtonBar.findViewById(R.id.button_primary).setId(R.id.editor_dialog_done_button);
        mButtonBar.findViewById(R.id.button_secondary).setId(R.id.payments_edit_cancel_button);

        prepareButtons();
    }

    public void setEditorTitle(String editorTitle) {
        assert editorTitle != null : "Editor title can't be null";
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        toolbar.setTitle(editorTitle);
    }

    public void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        if (customDoneButtonText != null) {
            mDoneButton.setText(customDoneButtonText);
        } else {
            mDoneButton.setText(mContext.getString(R.string.done));
        }
    }

    public void setDeleteConfirmationTitle(@Nullable String deleteConfirmationTitle) {
        mDeleteConfirmationTitle = deleteConfirmationTitle;
    }

    public void setDeleteConfirmationText(@Nullable CharSequence deleteConfirmationText) {
        mDeleteConfirmationText = deleteConfirmationText;
    }

    public void setDeleteConfirmationPrimaryButtonText(
            @Nullable String deleteConfirmationPrimaryButtonText) {
        mDeleteConfirmationPrimaryButtonText = deleteConfirmationPrimaryButtonText;
    }

    public void setAllowDelete(boolean allowDelete) {
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        toolbar.setShowDeleteMenuItem(allowDelete);
    }

    public void setDeleteRunnable(Runnable deleteRunnable) {
        mDeleteRunnable = deleteRunnable;
    }

    public void setDoneRunnable(Runnable doneRunnable) {
        mDoneRunnable = doneRunnable;
        setDoneRunnableToFields(doneRunnable);
    }

    private void setDoneRunnableToFields(Runnable doneRunnable) {
        for (FieldView view : mFieldViews) {
            if (view instanceof TextFieldView) {
                ((TextFieldView) view).setDoneRunnable(doneRunnable);
            }
        }
    }

    public void setCancelRunnable(Runnable cancelRunnable) {
        mCancelRunnable = cancelRunnable;
    }

    /**
     * Sets the suffix to be appended to the profile deletion histogram.
     *
     * @param suffix The suffix to append, e.g., the profile's record type.
     */
    public void setProfileRecordTypeSuffix(@Nullable String suffix) {
        mProfileRecordTypeSuffix = suffix;
    }

    public void setValidateOnShow(boolean validateOnShow) {
        mValidateOnShow = validateOnShow;
    }

    public void setVisible(boolean visible) {
        if (visible) {
            showDialog();
        } else {
            animateOutDialog();
        }
    }

    public void setShowButtons(boolean showButtons) {
        if (showButtons) {
            mButtonBar.setVisibility(View.VISIBLE);
        } else {
            mButtonBar.setVisibility(View.GONE);
        }
    }

    public void setEditorFields(ListModel<EditorItem> editorFields) {
        prepareEditor(editorFields);
    }

    /** Prevents screenshots of this editor. */
    public void disableScreenshots() {
        WindowManager.LayoutParams attributes = assumeNonNull(getWindow()).getAttributes();
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
        EditorDialogToolbar toolbar =
                (EditorDialogToolbar) mContainerView.findViewById(R.id.action_bar);
        toolbar.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(toolbar.getContext()));
        toolbar.setTitleTextAppearance(
                toolbar.getContext(), R.style.TextAppearance_Headline_Primary);

        // Show the help article when the help icon is clicked on, or delete
        // the profile and go back when the delete icon is clicked on.
        toolbar.setOnMenuItemClickListener(
                item -> {
                    if (item.getItemId() == R.id.delete_menu_id) {
                        if (mDeleteConfirmationTitle != null
                                && mDeleteConfirmationText != null
                                && mDeleteConfirmationPrimaryButtonText != null) {
                            handleDeleteWithConfirmation(
                                    mDeleteConfirmationTitle,
                                    mDeleteConfirmationText,
                                    mDeleteConfirmationPrimaryButtonText);
                        } else {
                            handleDelete();
                        }
                    } else if (item.getItemId() == R.id.help_menu_id) {
                        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                                .show(
                                        mActivity,
                                        mActivity.getString(R.string.help_context_autofill),
                                        null);
                    }
                    return true;
                });

        // Cancel editing when the user hits the back arrow.
        toolbar.setNavigationContentDescription(R.string.cancel);
        toolbar.setNavigationIcon(getTintedBackIcon());
        toolbar.setNavigationOnClickListener(view -> assumeNonNull(mCancelRunnable).run());

        // The top shadow is handled by the toolbar, so hide the one used in the field editor.
        FadingEdgeScrollView scrollView =
                (FadingEdgeScrollView) mContainerView.findViewById(R.id.scroll_view);
        scrollView.setEdgeVisibility(EdgeType.NONE, EdgeType.FADING);

        // The shadow's top margin doesn't get picked up in the xml; set it programmatically.
        View shadow = mContainerView.findViewById(R.id.shadow);
        LayoutParams params = (LayoutParams) shadow.getLayoutParams();
        params.topMargin = toolbar.getLayoutParams().height;
        shadow.setLayoutParams(params);
        scrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        SettingsUtils.getShowShadowOnScrollListener(scrollView, shadow));
    }

    @Override
    public void onClick(View view) {
        // Disable interaction during animation.
        if (mDialogInOutAnimator != null) return;

        if (view.getId() == R.id.editor_dialog_done_button) {
            assumeNonNull(mDoneRunnable).run();
        } else if (view.getId() == R.id.payments_edit_cancel_button) {
            assumeNonNull(mCancelRunnable).run();
        }
    }

    private void animateOutDialog() {
        if (mDialogInOutAnimator != null || !isShowing()) return;

        if (getCurrentFocus() != null) {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(getCurrentFocus());
        }

        Animator dropDown =
                ObjectAnimator.ofFloat(
                        mContainerView, View.TRANSLATION_Y, 0f, mContainerView.getHeight());
        Animator fadeOut =
                ObjectAnimator.ofFloat(mContainerView, View.ALPHA, mContainerView.getAlpha(), 0f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(dropDown, fadeOut);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_EXIT_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mDialogInOutAnimator.addListener(
                new AnimatorListenerAdapter() {
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
        removeTextChangedListeners();
    }

    private void prepareButtons() {
        mDoneButton = mButtonBar.findViewById(R.id.editor_dialog_done_button);
        mDoneButton.setOnClickListener(this);

        Button cancelButton = mButtonBar.findViewById(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);
    }

    /**
     * Create the visual representation of the PropertyModel defined by {@link EditorProperties}.
     *
     * <p>This would be more optimal as a RelativeLayout, but because it's dynamically generated,
     * it's much more human-parsable with inefficient LinearLayouts for half-width controls sharing
     * rows.
     *
     * @param editorFields the list of fields this editor should display.
     */
    private void prepareEditor(ListModel<EditorItem> editorFields) {
        // Ensure the layout is empty.
        removeTextChangedListeners();
        mContentView.removeAllViews();
        mFieldViews.clear();
        mTextFieldMCPs.forEach(PropertyModelChangeProcessor::destroy);
        mDropdownFieldMCPs.forEach(PropertyModelChangeProcessor::destroy);
        mTextFieldMCPs.clear();
        mDropdownFieldMCPs.clear();
        mEditableTextFields.clear();
        mDropdownFields.clear();

        // Add Views for each of the {@link EditorFields}.
        for (int i = 0; i < editorFields.size(); i++) {
            EditorItem editorItem = editorFields.get(i);
            EditorItem nextEditorItem = null;

            boolean isLastField = i == editorFields.size() - 1;
            boolean useFullLine = editorItem.isFullLine;
            if (!isLastField && !useFullLine) {
                // If the next field isn't full, stretch it out.
                nextEditorItem = editorFields.get(i + 1);
                if (nextEditorItem.isFullLine) useFullLine = true;
            }

            // Always keep dropdowns and text fields on different lines because of height
            // differences.
            if (!isLastField
                    && !useFullLine
                    && isDropdownField(editorItem)
                            != isDropdownField(assumeNonNull(nextEditorItem))) {
                useFullLine = true;
            }

            if (useFullLine || isLastField) {
                addFieldViewToEditor(mContentView, editorItem);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(mActivity);
                mContentView.addView(rowLayout);

                View firstView = addFieldViewToEditor(rowLayout, editorItem);
                View lastView = addFieldViewToEditor(rowLayout, assumeNonNull(nextEditorItem));

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
        setDoneRunnableToFields(assumeNonNull(mDoneRunnable));
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered
     * by adding padding to both sides.
     */
    public void onConfigurationChanged() {
        if (mUiConfig == null) {
            int minWidePaddingPixels =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);
            mUiConfig = new UiConfig(mContentView);
            ViewResizer.createAndAttach(mContentView, mUiConfig, 0, minWidePaddingPixels);
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

    private View addFieldViewToEditor(ViewGroup parent, final EditorItem editorItem) {
        View childView = null;

        switch (editorItem.type) {
            case DROPDOWN:
                {
                    DropdownFieldView dropdownView =
                            new DropdownFieldView(mContext, parent, editorItem.model);
                    mDropdownFieldMCPs.add(
                            PropertyModelChangeProcessor.create(
                                    editorItem.model,
                                    dropdownView,
                                    EditorDialogViewBinder::bindDropdownFieldView));
                    mFieldViews.add(dropdownView);
                    mDropdownFields.add(dropdownView.getDropdown());
                    childView = dropdownView.getLayout();
                    break;
                }
            case TEXT_INPUT:
                {
                    TextFieldView inputLayout = new TextFieldView(mContext, editorItem.model);
                    mTextFieldMCPs.add(
                            PropertyModelChangeProcessor.create(
                                    editorItem.model,
                                    inputLayout,
                                    EditorDialogViewBinder::bindTextFieldView));
                    mFieldViews.add(inputLayout);
                    mEditableTextFields.add(inputLayout.getEditText());
                    childView = inputLayout;
                    break;
                }
            case NON_EDITABLE_TEXT:
                {
                    View textLayout =
                            LayoutInflater.from(mContext)
                                    .inflate(
                                            R.layout.autofill_editor_dialog_non_editable_textview,
                                            null);
                    PropertyModelChangeProcessor.create(
                            editorItem.model,
                            textLayout,
                            EditorDialogViewBinder::bindNonEditableTextView);
                    childView = textLayout;
                    break;
                }
            case NOTICE:
                {
                    View noticeLayout =
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.autofill_editor_dialog_notice, null);
                    TextView textView = noticeLayout.findViewById(R.id.notice);
                    PropertyModelChangeProcessor.create(
                            editorItem.model, textView, EditorDialogViewBinder::bindNoticeTextView);
                    childView = noticeLayout;
                    break;
                }
        }
        assumeNonNull(childView);
        parent.addView(childView);
        return childView;
    }

    /** Displays the editor user interface for the given model. */
    private void showDialog() {
        // If an asynchronous task calls show, while the activity is already finishing, return.
        if (mActivity.isFinishing()) return;

        onConfigurationChanged();

        // Temporarily hide the content to avoid blink before animation starts.
        mContainerView.setVisibility(View.INVISIBLE);
        show();
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

        mContainerView.setVisibility(View.VISIBLE);
        mContainerView.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mContainerView.buildLayer();
        Animator popUp =
                ObjectAnimator.ofFloat(
                        mContainerView, View.TRANSLATION_Y, mContainerView.getHeight(), 0f);
        Animator fadeIn = ObjectAnimator.ofFloat(mContainerView, View.ALPHA, 0f, 1f);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(popUp, fadeIn);

        mDialogInOutAnimator = animatorSet;
        mDialogInOutAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
        mDialogInOutAnimator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mDialogInOutAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mContainerView.setLayerType(View.LAYER_TYPE_NONE, null);
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
        mHandler.post(
                () -> {
                    List<FieldView> invalidViews = new ArrayList<>();
                    if (mValidateOnShow) {
                        for (FieldView view : mFieldViews) {
                            if (!view.validate()) {
                                invalidViews.add(view);
                            }
                        }
                    }

                    // If TalkBack is enabled, we want to keep the focus at the top
                    // because the user would not learn about the elements that are
                    // above the focused field.
                    if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                        if (!invalidViews.isEmpty()) {
                            // Immediately focus the first invalid field to make it faster to edit.
                            invalidViews.get(0).scrollToAndFocus();
                        } else {
                            // Trigger default focus as it is not triggered automatically on Android
                            // P+.
                            mContainerView.requestFocus();
                        }
                    }
                    // Note that keyboard will not be shown for dropdown field since it's not
                    // necessary.
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
        assert mDeleteRunnable != null;
        mDeleteRunnable.run();
        animateOutDialog();
    }

    private void handleDeleteWithConfirmation(
            String confirmationTitle, CharSequence confirmationText, String primaryButtonText) {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View body = inflater.inflate(R.layout.confirmation_dialog_view, null);
        TextView titleView = body.findViewById(R.id.confirmation_dialog_title);
        titleView.setText(confirmationTitle);
        TextView messageView = body.findViewById(R.id.confirmation_dialog_message);
        messageView.setText(confirmationText);
        messageView.setMovementMethod(LinkMovementMethod.getInstance());

        // TODO(crbug.com/440257087): Migrate to modal dialog to keep it consistent with keyboard
        // accessory removal dialog.
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(body)
                        .setNegativeButton(
                                R.string.cancel,
                                (dialog, which) -> {
                                    recordDeletionHistogram(false);
                                    dialog.cancel();
                                    mConfirmationDialog = null;
                                    if (sObserverForTest != null) {
                                        sObserverForTest.onEditorReadyToEdit();
                                    }
                                })
                        .setPositiveButton(
                                primaryButtonText,
                                (dialog, which) -> {
                                    recordDeletionHistogram(true);
                                    handleDelete();
                                    mConfirmationDialog = null;
                                })
                        .create();
        mConfirmationDialog.show();

        if (sObserverForTest != null) {
            sObserverForTest.onEditorConfirmationDialogShown();
        }
    }

    /** @return The View with all fields of this editor. */
    public View getContentViewForTest() {
        return mContentView;
    }

    /** @return All editable text fields in the editor. Used only for tests. */
    public List<EditText> getEditableTextFieldsForTest() {
        return mEditableTextFields;
    }

    /** @return All dropdown fields in the editor. Used only for tests. */
    public List<Spinner> getDropdownFieldsForTest() {
        return mDropdownFields;
    }

    public @Nullable AlertDialog getConfirmationDialogForTest() {
        return mConfirmationDialog;
    }

    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        DropdownFieldView.setEditorObserverForTest(sObserverForTest);
        TextFieldView.setEditorObserverForTest(sObserverForTest);
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    private Drawable getTintedBackIcon() {
        return TintedDrawable.constructTintedDrawable(
                getContext(),
                R.drawable.ic_arrow_back_white_24dp,
                R.color.default_icon_color_tint_list);
    }

    private void recordDeletionHistogram(boolean deleted) {
        RecordHistogram.recordBooleanHistogram(PROFILE_DELETED_HISTOGRAM, deleted);
        RecordHistogram.recordBooleanHistogram(PROFILE_DELETED_SETTINGS_HISTOGRAM, deleted);

        if (mProfileRecordTypeSuffix != null && !mProfileRecordTypeSuffix.isEmpty()) {
            RecordHistogram.recordBooleanHistogram(
                    PROFILE_DELETED_HISTOGRAM + "." + mProfileRecordTypeSuffix, deleted);
            RecordHistogram.recordBooleanHistogram(
                    PROFILE_DELETED_SETTINGS_HISTOGRAM + "." + mProfileRecordTypeSuffix, deleted);
        }
    }
}
