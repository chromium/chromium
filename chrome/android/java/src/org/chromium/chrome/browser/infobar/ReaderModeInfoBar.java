// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;

import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.widget.text.AccessibleTextView;

/**
 * This is the InfoBar implementation of the Reader Mode UI. This is used in place of the
 * {@link OverlayPanel} implementation when Chrome Home is enabled.
 */
public class ReaderModeInfoBar extends InfoBar {
    /** If the infobar has started hiding. */
    private boolean mIsHiding;

    /**
     * Navigate to Reader Mode when the icon or the message text is clicked.
     */
    private View.OnClickListener mNavigateListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (getReaderModeManager() == null || mIsHiding) return;
            getReaderModeManager().activateReaderMode();
        }
    };

    /**
     * Default constructor.
     */
    private ReaderModeInfoBar() {
        super(R.drawable.infobar_mobile_friendly, R.color.infobar_icon_drawable_color, null, null);
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void onStartedHiding() {
        mIsHiding = true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        TextView prompt = new AccessibleTextView(getContext());
        prompt.setText(R.string.reader_view_text_alt);
        prompt.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                getContext().getResources().getDimension(R.dimen.infobar_text_size));
        prompt.setTextColor(
                ApiCompatibilityUtils.getColor(layout.getResources(), R.color.default_text_color));
        prompt.setGravity(Gravity.CENTER_VERTICAL);
        prompt.setOnClickListener(mNavigateListener);

        ImageView iconView = layout.findViewById(R.id.infobar_icon);
        iconView.setOnClickListener(mNavigateListener);
        iconView.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final int messagePadding = getContext().getResources().getDimensionPixelOffset(
                R.dimen.reader_mode_infobar_text_padding);
        prompt.setPadding(0, messagePadding, 0, messagePadding);
        layout.addContent(prompt, 1f);
    }

    @Override
    protected CharSequence getAccessibilityMessage(CharSequence defaultMessage) {
        return getContext().getString(R.string.reader_view_text_alt);
    }

    @Override
    public void onCloseButtonClicked() {
        if (getReaderModeManager() != null) {
            getReaderModeManager().onClosed(StateChangeReason.CLOSE_BUTTON);
        }
        super.onCloseButtonClicked();
    }

    /**
     * Create and show the Reader Mode {@link InfoBar}.
     * @param tab The tab that the {@link InfoBar} should be shown in.
     */
    public static void showReaderModeInfoBar(Tab tab) {
        ReaderModeInfoBarJni.get().create(tab);
    }

    /**
     * @return The {@link ReaderModeManager} for this infobar.
     */
    private ReaderModeManager getReaderModeManager() {
        if (getNativeInfoBarPtr() == 0) return null;
        Tab tab = ReaderModeInfoBarJni.get().getTab(getNativeInfoBarPtr(), ReaderModeInfoBar.this);

        if (tab == null || tab.getActivity() == null) return null;
        return tab.getActivity().getReaderModeManager();
    }

    /**
     * @return An instance of the {@link ReaderModeInfoBar}.
     */
    @CalledByNative
    private static ReaderModeInfoBar create() {
        return new ReaderModeInfoBar();
    }

    @NativeMethods
    interface Natives {
        void create(Tab tab);
        Tab getTab(long nativeReaderModeInfoBar, ReaderModeInfoBar caller);
    }
}
