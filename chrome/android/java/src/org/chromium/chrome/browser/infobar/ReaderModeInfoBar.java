// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;

import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.text.AccessibleTextView;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;

/** This is the InfoBar implementation of the Reader Mode UI. */
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
        prompt.setTextColor(AppCompatResources.getColorStateList(
                getContext(), R.color.default_text_color_list));
        prompt.setGravity(Gravity.CENTER_VERTICAL);
        prompt.setOnClickListener(mNavigateListener);

        ImageView iconView = layout.findViewById(R.id.infobar_icon);
        iconView.setOnClickListener(mNavigateListener);
        iconView.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final int messagePadding = getContext().getResources().getDimensionPixelOffset(
                R.dimen.infobar_compact_message_vertical_padding);
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
            getReaderModeManager().onClosed();
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

    /** @return The tab that this infobar is showing for. */
    private Tab getTab() {
        if (getNativeInfoBarPtr() == 0) return null;
        return ReaderModeInfoBarJni.get().getTab(getNativeInfoBarPtr(), ReaderModeInfoBar.this);
    }

    /**
     * @return The {@link ReaderModeManager} for this infobar.
     */
    private ReaderModeManager getReaderModeManager() {
        Tab tab = getTab();
        if (tab == null) return null;
        return tab.getUserDataHost().getUserData(ReaderModeManager.USER_DATA_KEY);
    }

    /**
     * @return An instance of the {@link ReaderModeInfoBar}.
     */
    @CalledByNative
    private static ReaderModeInfoBar create() {
        return new ReaderModeInfoBar();
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void create(Tab tab);
        Tab getTab(long nativeReaderModeInfoBar, ReaderModeInfoBar caller);
    }
}
