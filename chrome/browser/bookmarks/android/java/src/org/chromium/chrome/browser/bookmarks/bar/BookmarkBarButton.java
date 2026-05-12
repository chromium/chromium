// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/**
 * View for a button in the bookmark bar which provides users with bookmark access from top chrome.
 */
@NullMarked
class BookmarkBarButton extends LinearLayout {

    private ImageView mIcon;
    private int mLastEventMetaState;
    private int mLastEventButtonState;
    private TextView mTitle;

    private @Nullable ClickWithMetaStateCallback mClickCallback;
    private @Nullable CallbackController mIconCallbackController;

    /**
     * Constructor that is called when inflating a bookmark bar button from XML.
     *
     * @param context the context the bookmark bar button is running in.
     * @param attrs the attributes of the XML tag that is inflating the bookmark bar button.
     */
    public BookmarkBarButton(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIcon = findViewById(R.id.bookmark_bar_button_icon);
        mTitle = findViewById(R.id.bookmark_bar_button_title);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        // NOTE: Update `mLastEventMetaState` and `mLastEventButtonState` in anticipation of a
        // potential click.
        mLastEventMetaState = event.getMetaState();

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_BUTTON_PRESS) {
            mLastEventButtonState = event.getButtonState();
        }

        // Consume events for the middle button. Since we consume ACTION_DOWN for middle-clicks,
        // the standard OnClickListener (which only handles primary clicks) will not be triggered.
        if ((mLastEventButtonState & MotionEvent.BUTTON_TERTIARY) != 0) {
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                mLastEventButtonState = 0;
            }
            return true;
        }

        return super.onTouchEvent(event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        mLastEventMetaState = event.getMetaState();
        // Keyboard events do not have a mouse button state. Reset this to 0 to
        // ensure that if this key event triggers a click, it does not use a
        // stale button state from a previous mouse event.
        mLastEventButtonState = 0;
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        mLastEventMetaState = event.getMetaState();

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_BUTTON_PRESS) {
            mLastEventButtonState = event.getButtonState();
        }

        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            // Handle middle-click on release to match standard click behavior.
            // getActionButton() identifies the button that changed state.
            if (action == MotionEvent.ACTION_BUTTON_RELEASE
                    && event.getActionButton() == MotionEvent.BUTTON_TERTIARY) {
                // Manually set the button state to tertiary for the click callback, as
                // ACTION_BUTTON_RELEASE excludes the button being released from getButtonState().
                mLastEventButtonState = MotionEvent.BUTTON_TERTIARY;
                onClick(this);
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    /**
     * Sets the callback to notify of bookmark bar button click events (mouse clicks and finger
     * taps). The callback is provided the meta state and button state of the most recent key/touch
     * event.
     *
     * @param callback the callback to notify.
     */
    public void setClickCallback(@Nullable ClickWithMetaStateCallback callback) {
        mClickCallback = callback;
        if (callback == null) {
            setOnClickListener(null);
            return;
        }

        setOnClickListener(this::onClick);
    }

    private void onClick(View view) {
        if (mClickCallback != null) {
            mClickCallback.onClickWithMeta(mLastEventMetaState, mLastEventButtonState);
        }
    }

    /**
     * Sets the supplier for the icon to render in the bookmark bar button.
     *
     * @param iconSupplier the supplier for the icon to render.
     */
    public void setIconSupplier(@Nullable LazyOneshotSupplier<Drawable> iconSupplier) {
        if (mIconCallbackController != null) {
            mIconCallbackController.destroy();
            mIconCallbackController = null;
        }

        if (iconSupplier == null) {
            mIcon.setImageDrawable(null);
            return;
        }

        mIconCallbackController = new CallbackController();
        iconSupplier.onAvailable(mIconCallbackController.makeCancelable(mIcon::setImageDrawable));
        mIcon.setImageDrawable(iconSupplier.get());
    }

    /**
     * Sets the tint list of the icon to render in the bookmark bar button.
     *
     * @param id the resource identifier for the tint list.
     */
    public void setIconTintList(@ColorRes int id) {
        final ColorStateList tintList =
                id != Resources.ID_NULL ? getContext().getColorStateList(id) : null;

        if (ImageViewCompat.getImageTintList(mIcon) != tintList) {
            ImageViewCompat.setImageTintList(mIcon, tintList);
        }
    }

    /**
     * Sets the title to render in the bookmark bar button.
     *
     * @param title the title to render.
     */
    public void setTitle(@Nullable String title) {
        mTitle.setText(title);
    }

    /**
     * Sets the text appearance of the title.
     *
     * @param resId The style resource identifier.
     */
    public void setTitleTextAppearance(@StyleRes int resId) {
        mTitle.setTextAppearance(resId);
    }

    /**
     * @return the icon which is rendered in the bookmark bar button.
     */
    @Nullable Drawable getIconForTesting() {
        return mIcon.getDrawable();
    }

    /**
     * @return the tint list of the icon which is rendered in the bookmark bar button.
     */
    @Nullable ColorStateList getIconTintListForTesting() {
        return ImageViewCompat.getImageTintList(mIcon);
    }

    /**
     * @return the title which is rendered in the bookmark bar button.
     */
    @Nullable CharSequence getTitleForTesting() {
        return mTitle.getText();
    }
}
