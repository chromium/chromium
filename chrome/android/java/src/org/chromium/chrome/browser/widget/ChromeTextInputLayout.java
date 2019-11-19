// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableContainer;
import android.os.Build;
import android.support.design.widget.DrawableUtils;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.AccessibilityDelegateCompat;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.ViewPropertyAnimatorListenerAdapter;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v7.widget.AppCompatDrawableManager;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;

/**
 * A custom LinearLayout similar to Android's TextInputLayout. This class implements only a small
 * subset of features provided by TextInputLayout (floating animated label and error text) since we
 * don't need everything for Chrome, for now, and helps us avoid the TextInputLayout's binary size
 * impact by removing the dependency on it.
 *
 * WARNING: The child EditText's setOnFocusChangeListener() method shouldn't be called directly
 * because this class depends on the child's focus events. Instead, use
 * {@link ChromeTextInputLayout#addEditTextOnFocusChangeListener}.
 */
public class ChromeTextInputLayout extends LinearLayout {
    @IntDef({LabelStatus.COLLAPSED, LabelStatus.EXPANDED, LabelStatus.ANIMATING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface LabelStatus {
        int COLLAPSED = 0;
        int EXPANDED = 1;
        int ANIMATING = 2;
    }

    /**
     * Focus change listener interface to use with the {@link EditText} inside the
     * {@link ChromeTextInputLayout}
     */
    public interface OnEditTextFocusChangeListener {
        void onEditTextFocusChange(View v, boolean hasFocus);
    }

    private static final long ANIMATION_DURATION_MS = 150;

    private EditText mEditText;
    private TextView mLabel;
    private TextView mErrorText;
    private FrameLayout mFrame;
    private HashSet<OnEditTextFocusChangeListener> mListeners = new HashSet<>();

    private CharSequence mHint;

    private boolean mShouldDisplayError;
    private @LabelStatus int mLabelStatus;

    private ColorStateList mDefaultLineColor;
    private @ColorInt int mErrorColor;
    private float mExpandedTextScale;
    private float mCollapsedLabelTranslationY;
    private float mExpandedLabelTranslationY;

    // Needed for #ensureBackgroundDrawableStateWorkaround().
    private boolean mHasReconstructedEditTextBackground;

    public ChromeTextInputLayout(Context context) {
        this(context, null);
    }

    public ChromeTextInputLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ChromeTextInputLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setOrientation(VERTICAL);

        final TypedArray a =
                context.obtainStyledAttributes(attrs, R.styleable.ChromeTextInputLayout);
        mHint = a.getText(R.styleable.ChromeTextInputLayout_hint);

        mLabel = new TextView(context);
        mLabel.setPaddingRelative(
                getResources().getDimensionPixelSize(R.dimen.text_input_layout_padding_start), 0, 0,
                0);
        ApiCompatibilityUtils.setTextAppearance(mLabel,
                a.getResourceId(R.styleable.ChromeTextInputLayout_hintTextAppearance,
                        R.style.TextAppearance_BlackCaption));
        mLabel.setPivotX(0f);
        mLabel.setPivotY(mLabel.getPaint().getFontMetrics().bottom);
        mLabelStatus = LabelStatus.COLLAPSED;
        addView(mLabel, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        mCollapsedLabelTranslationY = mLabel.getPaint().getFontMetrics().descent;
        mLabel.setTranslationY(mCollapsedLabelTranslationY);

        // A FrameLayout is put in the place of the EditText when the class is first instantiated.
        // When #addView is called with an EditText, whether because the layout is inflated from an
        // xml or an EditText is added programmatically, this FrameLayout is replaced with the
        // EditText.
        mFrame = new FrameLayout(context);
        addView(mFrame, -1);

        mErrorText = new TextView(context);
        mErrorText.setTextAppearance(getContext(),
                a.getResourceId(R.styleable.ChromeTextInputLayout_errorTextAppearance,
                        R.style.TextAppearance_ErrorCaption));
        mErrorText.setVisibility(View.GONE);
        mErrorText.setPaddingRelative(
                getResources().getDimensionPixelSize(R.dimen.text_input_layout_padding_start), 0, 0,
                0);
        mErrorColor = mErrorText.getCurrentTextColor();
        addView(mErrorText, -1);

        a.recycle();

        mDefaultLineColor = new ColorStateList(
                new int[][] {new int[] {android.R.attr.state_focused}, new int[] {}},
                new int[] {getColorAttribute(context, R.attr.colorControlActivated),
                        getColorAttribute(context, R.attr.colorControlNormal)});

        mLabel.setTextColor(new ColorStateList(
                new int[][] {new int[] {android.R.attr.state_activated}, new int[] {}},
                new int[] {getColorAttribute(context, R.attr.colorControlActivated),
                        mLabel.getCurrentTextColor()}));

        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        ViewCompat.setAccessibilityDelegate(this, new AccessibilityDelegate());
    }

    /**
     * Set the error to display below the EditText. Passing an empty {@link CharSequence} or null
     * clears the error.
     * @param error The error text.
     */
    public void setError(CharSequence error) {
        if (TextUtils.isEmpty(error) && TextUtils.isEmpty(mErrorText.getText())) {
            return;
        }

        assert mEditText != null;
        mShouldDisplayError = !TextUtils.isEmpty(error);
        mErrorText.setText(error);
        mErrorText.setVisibility(mShouldDisplayError ? View.VISIBLE : View.GONE);

        // Fixes issues with Android L where the line's error color wouldn't be cleared after
        // leaving the activity; and KitKat where the line wouldn't get tinted when showing an
        // error. crbug.com/1020077.
        ensureBackgroundDrawableStateWorkaround();
        if (mShouldDisplayError) {
            // Tinting doesn't really work on KitKat, so use a color filter instead.
            mEditText.getBackground().setColorFilter(
                    AppCompatDrawableManager.getPorterDuffColorFilter(
                            mErrorColor, PorterDuff.Mode.SRC_IN));
        } else {
            DrawableCompat.clearColorFilter(mEditText.getBackground());
            mEditText.refreshDrawableState();
        }

        ViewCompat.setAccessibilityLiveRegion(
                mErrorText, ViewCompat.ACCESSIBILITY_LIVE_REGION_POLITE);
        updateLabelState(true);
    }

    /**
     * @return The error text.
     */
    public CharSequence getError() {
        return mErrorText.getText();
    }

    /**
     * @return The {@link android.widget.EditText}.
     */
    public EditText getEditText() {
        return mEditText;
    }

    /**
     * Set the hint to be displayed in the floating label.
     * @param hint The hint text.
     */
    public void setHint(CharSequence hint) {
        mHint = hint;
        updateLabel();
        // TODO(sinansahin): We're mirroring the Android code here, but investigate if we need to
        // send this event, probably after updating the support library.
        sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        updateLabelState(false);
    }

    /**
     * @return The hint text.
     */
    public CharSequence getHint() {
        return mHint;
    }

    /**
     * Add a {@link OnEditTextFocusChangeListener}. This method should be used instead of
     * {@link EditText#setOnFocusChangeListener}.
     * @param listener The {@link OnEditTextFocusChangeListener} to add.
     */
    public void addEditTextOnFocusChangeListener(OnEditTextFocusChangeListener listener) {
        mListeners.add(listener);
    }

    /**
     * Remove a {@link OnEditTextFocusChangeListener}.
     * @param listener The {@link OnEditTextFocusChangeListener} to remove.
     */
    public void removeEditTextOnFocusChangeListener(OnEditTextFocusChangeListener listener) {
        mListeners.remove(listener);
    }

    @Override
    public final void addView(View child, int index, ViewGroup.LayoutParams params) {
        if (child instanceof EditText) {
            setEditText((EditText) child);

            // Replace the placeholder FrameLayout with the real EditText.
            ViewGroup parent = (ViewGroup) mFrame.getParent();
            index = parent.indexOfChild(mFrame);
            parent.removeView(mFrame);
            mFrame = null;
        }

        // Carry on adding the View.
        super.addView(child, index, params);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (mExpandedLabelTranslationY == 0) {
            mExpandedLabelTranslationY = mEditText.getBaseline();
            if (mLabelStatus == LabelStatus.EXPANDED) {
                mLabel.setTranslationY(mExpandedLabelTranslationY);
            }
        }
    }

    private void updateLabel() {
        mLabel.setText(mHint);
        final Rect bounds = new Rect();
        mLabel.getPaint().getTextBounds(
                mLabel.getText().toString(), 0, mLabel.getText().length(), bounds);
        final float width = bounds.width();
        mLabel.setPivotX(LocalizationUtils.isLayoutRtl() ? width : 0);
    }

    private void setEditText(EditText editText) {
        // If we already have an EditText, throw an exception.
        if (mEditText != null) {
            throw new IllegalArgumentException("We already have an EditText, can only have one");
        }
        mEditText = editText;
        DrawableCompat.setTintList(mEditText.getBackground().mutate(), mDefaultLineColor);
        mExpandedTextScale = mEditText.getTextSize() / mLabel.getTextSize();
        mEditText.setOnFocusChangeListener((v, hasFocus) -> {
            notifyEditTextFocusChanged(v, hasFocus);
            updateLabelState(true);
        });
        mEditText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                updateLabelState(false);
            }
        });

        // If we do not have a valid hint, try and retrieve it from the EditText.
        if (TextUtils.isEmpty(mHint)) {
            setHint(mEditText.getHint());
            mEditText.setHint(null);
        }

        updateLabel();
        updateLabelState(false);
    }

    private void updateLabelState(boolean animate) {
        final boolean hasText = !TextUtils.isEmpty(mEditText.getText());
        final boolean isFocused = mEditText.isFocused();

        mLabel.setActivated(isFocused);

        if (mLabelStatus == LabelStatus.ANIMATING) {
            mLabel.clearAnimation();
        }
        if (hasText || isFocused || mShouldDisplayError) {
            // The label should be collapsed.
            collapseLabel(animate);
        } else {
            // The label should be expanded.
            expandLabel(animate);
        }
    }

    /**
     * Collapse the label (shrink and move up).
     * @param animate Whether we should animate the label collapsing.
     */
    private void collapseLabel(boolean animate) {
        if (animate) {
            ViewCompat.animate(mLabel)
                    .translationY(mCollapsedLabelTranslationY)
                    .scaleY(1f)
                    .scaleX(1f)
                    .setDuration(ANIMATION_DURATION_MS)
                    .setListener(new ViewPropertyAnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(View view) {
                            mLabelStatus = LabelStatus.COLLAPSED;
                        }
                    })
                    .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                    .start();
        } else {
            mLabel.setTranslationY(mCollapsedLabelTranslationY);
            mLabel.setScaleX(1f);
            mLabel.setScaleY(1f);
            mLabelStatus = LabelStatus.COLLAPSED;
        }
    }

    /**
     * Expand the label (move down where the hint should be).
     * @param animate Whether we should animate the label expanding.
     */
    private void expandLabel(boolean animate) {
        if (animate) {
            ViewCompat.animate(mLabel)
                    .translationY(mExpandedLabelTranslationY)
                    .setDuration(ANIMATION_DURATION_MS)
                    .scaleX(mExpandedTextScale)
                    .scaleY(mExpandedTextScale)
                    .setListener(new ViewPropertyAnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(View view) {
                            mLabelStatus = LabelStatus.EXPANDED;
                        }
                    })
                    .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                    .start();
        } else {
            mLabel.setScaleX(mExpandedTextScale);
            mLabel.setScaleY(mExpandedTextScale);
            mLabel.setTranslationY(mExpandedLabelTranslationY);
            mLabelStatus = LabelStatus.EXPANDED;
        }
    }

    private void notifyEditTextFocusChanged(View v, boolean hasFocus) {
        for (OnEditTextFocusChangeListener listener : mListeners) {
            listener.onEditTextFocusChange(v, hasFocus);
        }
    }

    /**
     * Returns a color from a theme attribute.
     * @param context Context with resources to look up.
     * @param attribute Attribute such as R.attr.colorControlNormal.
     * @return Color value.
     * @throws Resources.NotFoundException
     */
    private int getColorAttribute(Context context, int attribute) {
        TypedValue typedValue = new TypedValue();
        if (!context.getTheme().resolveAttribute(attribute, typedValue, true)) {
            throw new Resources.NotFoundException("Attribute not found.");
        }

        if (typedValue.resourceId != 0) {
            // Attribute is a resource.
            return ContextCompat.getColor(context, typedValue.resourceId);
        } else if (typedValue.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && typedValue.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            // Attribute is a raw color value.
            return typedValue.data;
        } else {
            throw new Resources.NotFoundException("Attribute not a color.");
        }
    }

    /**
     * An AccessibilityDelegate intended to be set on an {@link EditText} to provide attributes for
     * accessibility that are managed by {@link ChromeTextInputLayout}.
     */
    private class AccessibilityDelegate extends AccessibilityDelegateCompat {
        AccessibilityDelegate() {}

        @Override
        public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfoCompat info) {
            super.onInitializeAccessibilityNodeInfo(host, info);
            // TODO(sinansahin): Update this method to use info.setHintText() and
            // info.setShowingHintText() once we update the support library to version 28.
            if (mEditText != null) {
                setLabelFor(mEditText.getId());
            }
            if (mShouldDisplayError) {
                info.setContentInvalid(true);
                info.setError(getError());
            }
        }
    }

    /**
     * Workaround for issues with Android K and L. Borrowed from the support library:
     * https://android.googlesource.com/platform/frameworks/support/+/refs/heads/oreo-r6-release/
     * design/src/android/support/design/widget/TextInputLayout.java
     */
    private void ensureBackgroundDrawableStateWorkaround() {
        final int sdk = Build.VERSION.SDK_INT;
        if (sdk > Build.VERSION_CODES.LOLLIPOP_MR1) {
            // The workaround is only required on API 22 and below.
            return;
        }

        final Drawable bg = mEditText.getBackground();
        if (bg == null) {
            return;
        }

        if (!mHasReconstructedEditTextBackground) {
            // This is gross. There is an issue in the platform which affects container Drawables
            // where the first drawable retrieved from resources will propagate any changes
            // (like color filter) to all instances from the cache. We'll try to workaround it...

            final Drawable newBg = bg.getConstantState().newDrawable();

            if (bg instanceof DrawableContainer) {
                // If we have a Drawable container, we can try and set it's constant state via
                // reflection from the new Drawable
                mHasReconstructedEditTextBackground = DrawableUtils.setContainerConstantState(
                        (DrawableContainer) bg, newBg.getConstantState());
            }

            if (!mHasReconstructedEditTextBackground) {
                // If we reach here then we just need to set a brand new instance of the Drawable
                // as the background. This has the unfortunate side-effect of wiping out any
                // user set padding, but I'd hope that use of custom padding on an EditText
                // is limited.
                ViewCompat.setBackground(mEditText, newBg);
                mHasReconstructedEditTextBackground = true;
            }
        }
    }
}
