// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import android.text.method.LinkMovementMethod;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldView;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@NullMarked
public abstract class EditorViewBase extends AlwaysDismissedDialog
        implements OnClickListener,
                DialogInterface.OnShowListener,
                DialogInterface.OnDismissListener {
    /** Duration of the animation to show the UI to full height. */
    private static final int DIALOG_ENTER_ANIMATION_MS = 300;

    /** Duration of the animation to hide the UI. */
    private static final int DIALOG_EXIT_ANIMATION_MS = 195;

    protected @Nullable static EditorObserverForTest sObserverForTest;

    private final Activity mActivity;
    private final Context mContext;

    private final View mContainerView;
    private final ViewGroup mContentView;

    private final List<FieldView> mFieldViews = new ArrayList<>();

    private boolean mIsDismissed;

    private final View mButtonBar;
    private Button mDoneButton;

    private @Nullable Animator mDialogInOutAnimator;
    private @Nullable AlertDialog mConfirmationDialog;

    private @Nullable String mDeleteConfirmationTitle;
    private @Nullable CharSequence mDeleteConfirmationText;
    private @StringRes int mDeleteConfirmationPrimaryButtonText;

    private @Nullable Callback<Activity> mOpenHelpCallback;

    private @Nullable Runnable mDeleteRunnable;
    private @Nullable Runnable mDoneRunnable;
    private @Nullable Runnable mCancelRunnable;

    private @Nullable UiConfig mUiConfig;

    public EditorViewBase(Activity activity) {
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

        mContainerView =
                LayoutInflater.from(mContext).inflate(R.layout.autofill_editor_dialog, null);
        setContentView(mContainerView);

        mContentView = mContainerView.findViewById(R.id.contents);

        prepareToolbar();

        mButtonBar = mContainerView.findViewById(R.id.button_bar);
        mButtonBar.findViewById(R.id.button_primary).setId(R.id.editor_dialog_done_button);
        mButtonBar.findViewById(R.id.button_secondary).setId(R.id.payments_edit_cancel_button);

        prepareButtons();

        setOnShowListener(this);
        setOnDismissListener(this);
    }

    public void setVisible(boolean visible) {
        if (visible) {
            showDialog();
        } else {
            animateOutDialog();
        }
    }

    /** Prevents screenshots of this editor. */
    public void disableScreenshots() {
        WindowManager.LayoutParams attributes = assumeNonNull(getWindow()).getAttributes();
        attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
        getWindow().setAttributes(attributes);
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

    public void setEditorFields(ListModel<EditorItem> editorFields) {
        prepareEditor(editorFields);
        setDoneRunnableToFields();
    }

    public List<FieldView> getFieldViews() {
        return Collections.unmodifiableList(mFieldViews);
    }

    public void addFieldView(FieldView fieldView) {
        mFieldViews.add(fieldView);
    }

    public void clearFieldViews() {
        removeTextChangedListeners();
        mContentView.removeAllViews();
        mFieldViews.clear();
    }

    public void setEditorTitle(String editorTitle) {
        assert editorTitle != null : "Editor title can't be null";
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        toolbar.setTitle(editorTitle);
    }

    public void setShowButtons(boolean showButtons) {
        if (showButtons) {
            mButtonBar.setVisibility(View.VISIBLE);
        } else {
            mButtonBar.setVisibility(View.GONE);
        }
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
            @StringRes int deleteConfirmationPrimaryButtonText) {
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
        setDoneRunnableToFields();
    }

    private void setDoneRunnableToFields() {
        for (FieldView view : mFieldViews) {
            if (view instanceof TextFieldView) {
                ((TextFieldView) view).setDoneRunnable(assumeNonNull(mDoneRunnable));
            }
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

    public void setCancelRunnable(Runnable cancelRunnable) {
        mCancelRunnable = cancelRunnable;
    }

    public void setOpenHelpCallback(@Nullable Callback<Activity> openHelpCallback) {
        mOpenHelpCallback = openHelpCallback;
    }

    /**
     * When this layout has a wide display style, it will be width constrained to {@link
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered by
     * adding padding to both sides.
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
        onEntryAnimationStart();

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
                        onEntryAnimationEnd();
                        mDialogInOutAnimator = null;
                        initFocus();
                    }
                });

        mDialogInOutAnimator.start();
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

    private void prepareButtons() {
        mDoneButton = mButtonBar.findViewById(R.id.editor_dialog_done_button);
        mDoneButton.setOnClickListener(this);

        Button cancelButton = mButtonBar.findViewById(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);
    }

    /**
     * Prepares the toolbar for use.
     *
     * <p>Many of the things that would ideally be set as attributes don't work and need to be set
     * programmatically. This is likely due to how we compile the support libraries.
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
                                && mDeleteConfirmationPrimaryButtonText != 0) {
                            handleDeleteWithConfirmation(
                                    mDeleteConfirmationTitle,
                                    mDeleteConfirmationText,
                                    mDeleteConfirmationPrimaryButtonText);
                        } else {
                            handleDelete();
                        }
                    } else if (item.getItemId() == R.id.help_menu_id) {
                        assumeNonNull(mOpenHelpCallback).onResult(mActivity);
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

    private Drawable getTintedBackIcon() {
        return TintedDrawable.constructTintedDrawable(
                getContext(),
                R.drawable.ic_arrow_back_white_24dp,
                R.color.default_icon_color_tint_list);
    }

    private void handleDelete() {
        assert mDeleteRunnable != null;
        mDeleteRunnable.run();
        animateOutDialog();
    }

    private void handleDeleteWithConfirmation(
            String confirmationTitle,
            CharSequence confirmationText,
            @StringRes int primaryButtonTextId) {
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
                                    recordDeletionHistogram(/* deleted= */ false);
                                    dialog.cancel();
                                    mConfirmationDialog = null;
                                    if (sObserverForTest != null) {
                                        sObserverForTest.onEditorReadyToEdit();
                                    }
                                })
                        .setPositiveButton(
                                primaryButtonTextId,
                                (dialog, which) -> {
                                    recordDeletionHistogram(/* deleted= */ true);
                                    handleDelete();
                                    mConfirmationDialog = null;
                                })
                        .create();
        mConfirmationDialog.show();

        if (sObserverForTest != null) {
            sObserverForTest.onEditorConfirmationDialogShown();
        }
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

    /**
     * Updates the list of editor fields displayed to the user.
     *
     * @param editorFields A list of editor field models.
     */
    protected abstract void prepareEditor(ListModel<EditorItem> editorFields);

    /** Called when the editor starts appearing on the screen. */
    protected abstract void onEntryAnimationStart();

    /** Called when the editor has finished appearing on the screen. */
    protected abstract void onEntryAnimationEnd();

    /** Called when the editor is shown to initialize the view focus. */
    protected abstract void initFocus();

    /** Called when the user attempted to delete the edited entry. */
    protected abstract void recordDeletionHistogram(boolean deleted);

    public static void setEditorObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        DropdownFieldView.setEditorObserverForTest(sObserverForTest);
        TextFieldView.setEditorObserverForTest(sObserverForTest);
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    /**
     * @return The Activity used to display this editor.
     */
    public Activity getActivity() {
        return mActivity;
    }

    /**
     * @return The Context used to display this editor. This context can be wrapped into the {@link
     *     ContextThemeWrapper} to modify the editor's visual appearance.
     */
    public Context getStyledContext() {
        return mContext;
    }

    /**
     * @return The View with all the content of this editor.
     */
    public View getContainerView() {
        return mContainerView;
    }

    /**
     * @return The View with all fields of this editor.
     */
    public ViewGroup getContentView() {
        return mContentView;
    }

    public @Nullable AlertDialog getConfirmationDialogForTest() {
        return mConfirmationDialog;
    }
}
