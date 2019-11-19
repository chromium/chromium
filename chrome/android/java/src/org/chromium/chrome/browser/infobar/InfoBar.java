// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The base class for all InfoBar classes.
 * Note that infobars expire by default when a new navigation occurs.
 * Make sure to use setExpireOnNavigation(false) if you want an infobar to be sticky.
 */
public abstract class InfoBar implements InfoBarView {
    private static final String TAG = "InfoBar";

    private final int mIconDrawableId;
    private final Bitmap mIconBitmap;
    private final @ColorRes int mIconTintId;
    private final CharSequence mMessage;

    private @Nullable InfoBarContainer mContainer;
    private @Nullable View mView;
    private @Nullable Context mContext;

    private boolean mIsDismissed;
    private boolean mControlsEnabled = true;

    private @Nullable PropertyModel mModel;

    // This points to the InfoBarAndroid class not any of its subclasses.
    private long mNativeInfoBarPtr;

    /**
     * Constructor for regular infobars.
     * @param iconDrawableId ID of the resource to use for the Icon.  If 0, no icon will be shown.
     * @param iconTintId The {@link ColorRes} used as tint for the {@code iconDrawableId}.
     * @param message The message to show in the infobar.
     * @param iconBitmap Icon to draw, in bitmap form.  Used mainly for generated icons.
     */
    public InfoBar(
            int iconDrawableId, @ColorRes int iconTintId, CharSequence message, Bitmap iconBitmap) {
        mIconDrawableId = iconDrawableId;
        mIconBitmap = iconBitmap;
        mIconTintId = iconTintId;
        mMessage = message;
    }

    /**
     * Stores a pointer to the native-side counterpart of this InfoBar.
     * @param nativeInfoBarPtr Pointer to the native InfoBarAndroid, not to its subclass.
     */
    @CalledByNative
    private final void setNativeInfoBar(long nativeInfoBarPtr) {
        mNativeInfoBarPtr = nativeInfoBarPtr;
    }

    @CalledByNative
    protected void onNativeDestroyed() {
        mNativeInfoBarPtr = 0;
    }

    public SnackbarManager getSnackbarManager() {
        return mContainer != null ? mContainer.getSnackbarManager() : null;
    }

    /**
     * Sets the Context used when creating the InfoBar.
     */
    protected void setContext(Context context) {
        mContext = context;
    }

    /**
     * @return The {@link Context} used to create the InfoBar. This will be null before the InfoBar
     *         is added to an {@link InfoBarContainer}, or after the InfoBar is closed.
     */
    @Nullable
    protected Context getContext() {
        return mContext;
    }

    /**
     * Creates the View that represents the InfoBar.
     * @return The View representing the InfoBar.
     */
    protected final View createView() {
        assert mContext != null;

        if (usesCompactLayout()) {
            InfoBarCompactLayout layout = new InfoBarCompactLayout(
                    mContext, this, mIconDrawableId, mIconTintId, mIconBitmap);
            createCompactLayoutContent(layout);
            mView = layout;
        } else {
            InfoBarLayout layout = new InfoBarLayout(
                    mContext, this, mIconDrawableId, mIconTintId, mIconBitmap, mMessage);
            createContent(layout);
            layout.onContentCreated();
            mView = layout;
        }

        return mView;
    }

    /**
     * @return The model for this infobar if one was created.
     */
    @Nullable
    PropertyModel getModel() {
        return mModel;
    }

    /**
     * If this returns true, the infobar contents will be replaced with a one-line layout.
     * When overriding this, also override {@link #getAccessibilityMessage}.
     */
    protected boolean usesCompactLayout() {
        return false;
    }

    /**
     * Prepares and inserts views into an {@link InfoBarCompactLayout}.
     * {@link #usesCompactLayout} must return 'true' for this function to be called.
     * @param layout Layout to plug views into.
     */
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {}

    /**
     * Replaces the View currently shown in the infobar with the given View. Triggers the swap
     * animation via the InfoBarContainer.
     */
    protected void replaceView(View newView) {
        mView = newView;
        mContainer.notifyInfoBarViewChanged();
    }

    /**
     * Returns the View shown in this infobar. Only valid after createView() has been called.
     */
    @Override
    public View getView() {
        return mView;
    }

    /**
     * Returns the accessibility message to announce when this infobar is first shown.
     * Override this if the InfoBar doesn't have {@link R.id.infobar_message}. It is usually the
     * case when it is in CompactLayout.
     */
    protected CharSequence getAccessibilityMessage(CharSequence defaultTitle) {
        return defaultTitle == null ? "" : defaultTitle;
    }

    @Override
    public CharSequence getAccessibilityText() {
        if (mView == null) return "";

        CharSequence title = null;
        TextView messageView = (TextView) mView.findViewById(R.id.infobar_message);
        if (messageView != null) {
            title = messageView.getText();
        }
        title = getAccessibilityMessage(title);
        if (title.length() > 0) {
            title = title + " ";
        }
        // TODO(crbug/773717): Avoid string concatenation due to i18n.
        return title + mContext.getString(R.string.bottom_bar_screen_position);
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.PAGE_TRIGGERED;
    }

    @Override
    @InfoBarIdentifier
    public int getInfoBarIdentifier() {
        if (mNativeInfoBarPtr == 0) return InfoBarIdentifier.INVALID;
        return InfoBarJni.get().getInfoBarIdentifier(mNativeInfoBarPtr, InfoBar.this);
    }

    /**
     * @return whether the infobar actually needed closing.
     */
    @CalledByNative
    private boolean closeInfoBar() {
        if (!mIsDismissed) {
            mIsDismissed = true;
            if (!mContainer.hasBeenDestroyed()) {
                // If the container was destroyed, it's already been emptied of all its infobars.
                onStartedHiding();
                mContainer.removeInfoBar(this);
            }
            mContainer = null;
            mView = null;
            mContext = null;
            return true;
        }
        return false;
    }

    /**
     * @return If the infobar is the front infobar (i.e. visible and not hidden behind other
     *         infobars).
     */
    public boolean isFrontInfoBar() {
        return mContainer.getFrontInfoBar() == this;
    }

    /**
     * Called just before the Java infobar has begun hiding.  Give the chance to clean up any child
     * UI that may remain open.
     */
    protected void onStartedHiding() {}

    long getNativeInfoBarPtr() {
        return mNativeInfoBarPtr;
    }

    void setInfoBarContainer(InfoBarContainer container) {
        mContainer = container;
    }

    /**
     * @return Whether or not this InfoBar is already dismissed (i.e. closed).
     */
    boolean isDismissed() {
        return mIsDismissed;
    }

    @Override
    public boolean areControlsEnabled() {
        return mControlsEnabled;
    }

    @Override
    public void setControlsEnabled(boolean state) {
        mControlsEnabled = state;
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
    }

    @Override
    public void onLinkClicked() {
        if (mNativeInfoBarPtr != 0) InfoBarJni.get().onLinkClicked(mNativeInfoBarPtr, InfoBar.this);
    }

    /**
     * Performs some action related to the button being clicked.
     * @param action The type of action defined in {@link ActionType} in this class.
     */
    protected void onButtonClicked(@ActionType int action) {
        if (mNativeInfoBarPtr != 0) {
            InfoBarJni.get().onButtonClicked(mNativeInfoBarPtr, InfoBar.this, action);
        }
    }

    @Override
    public void onCloseButtonClicked() {
        if (mNativeInfoBarPtr != 0 && !mIsDismissed) {
            InfoBarJni.get().onCloseButtonClicked(mNativeInfoBarPtr, InfoBar.this);
        }
    }

    @Override
    public void createContent(InfoBarLayout layout) {
    }

    @InfoBarIdentifier

    @NativeMethods
    interface Natives {
        int getInfoBarIdentifier(long nativeInfoBarAndroid, InfoBar caller);
        void onLinkClicked(long nativeInfoBarAndroid, InfoBar caller);
        void onButtonClicked(long nativeInfoBarAndroid, InfoBar caller, int action);
        void onCloseButtonClicked(long nativeInfoBarAndroid, InfoBar caller);
    }
}
