// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.survey.SurveyController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * An {@link InfoBar} that prompts the user to take an optional survey.
 */
public class SurveyInfoBar extends InfoBar {
    // The site to pull a survey from.
    private final String mSiteId;

    // Whether to display the survey as a bottom sheet.
    private final boolean mShowAsBottomSheet;

    // The display logo to be shown on the survey and this infobar.
    private final int mDisplayLogoResId;

    // The delegate to handle what happens when info bar events are triggered.
    private final SurveyInfoBarDelegate mDelegate;

    // Boolean to track if the infobar was clicked to prevent double triggering of the survey.
    private boolean mClicked;

    // Boolean to track if the infobar was closed via survey acceptance or onCloseButtonClicked() to
    // prevent onStartHiding() from being called after.
    private boolean mClosedByInteraction;

    // The clickable span that triggers the call to #showSurvey on the text prompt.
    private NoUnderlineClickableSpan mClickableSpan;

    /**
     * Create and show the {@link SurveyInfoBar}.
     * @param webContents The webcontents to create the {@link InfoBar} around.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param surveyInfoBarDelegate The delegate to customize what the infobar will do.
     */
    public static void showSurveyInfoBar(WebContents webContents, String siteId,
            boolean showAsBottomSheet, int displayLogoResId,
            SurveyInfoBarDelegate surveyInfoBarDelegate) {
        SurveyInfoBarJni.get().create(
                webContents, siteId, showAsBottomSheet, displayLogoResId, surveyInfoBarDelegate);
    }

    /**
     * Default constructor.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param showAsBottomSheet Whether the survey should be presented as a bottom sheet or not.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param surveyInfoBarDelegate The delegate to customize what happens when different events in
     *                              SurveyInfoBar are triggered.
     */
    private SurveyInfoBar(String siteId, boolean showAsBottomSheet, int displayLogoResId,
            SurveyInfoBarDelegate surveyInfoBarDelegate) {
        super(displayLogoResId, 0, null, null);

        mSiteId = siteId;
        mShowAsBottomSheet = showAsBottomSheet;
        mDisplayLogoResId = displayLogoResId;
        mDelegate = surveyInfoBarDelegate;
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        Tab tab = SurveyInfoBarJni.get().getTab(getNativeInfoBarPtr(), SurveyInfoBar.this);
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                mDelegate.onSurveyInfoBarTabHidden();
                tab.removeObserver(this);

                // Closes the infobar without calling the {@link SurveyInfoBarDelegate}'s
                // onSurveyInfoBarCloseButtonClicked.
                SurveyInfoBar.super.onCloseButtonClicked();
                // TODO(mdjones): add a proper close method to programatically close the infobar.
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                mDelegate.onSurveyInfoBarTabInteractabilityChanged(isInteractable);
            }
        });

        mClickableSpan = new NoUnderlineClickableSpan(layout.getResources(), (widget) -> {
            // Prevent double clicking on the text span.
            if (mClicked) return;
            showSurvey(tab);
            mClosedByInteraction = true;
        });

        CharSequence infoBarText = SpanApplier.applySpans(mDelegate.getSurveyPromptString(),
                new SpanInfo("<LINK>", "</LINK>", mClickableSpan));

        TextView prompt = new TextView(getContext());
        prompt.setText(infoBarText);
        prompt.setMovementMethod(LinkMovementMethod.getInstance());
        prompt.setGravity(Gravity.CENTER_VERTICAL);
        ApiCompatibilityUtils.setTextAppearance(prompt, R.style.TextAppearance_TextLarge_Primary);
        addAccessibilityClickListener(prompt, tab);
        layout.addContent(prompt, 1f);
    }

    @Override
    public void onCloseButtonClicked() {
        super.onCloseButtonClicked();
        mDelegate.onSurveyInfoBarClosed(true, true);
        mClosedByInteraction = true;
    }

    @CalledByNative
    private static SurveyInfoBar create(String siteId, boolean showAsBottomSheet,
            int displayLogoResId, SurveyInfoBarDelegate surveyInfoBarDelegate) {
        return new SurveyInfoBar(
                siteId, showAsBottomSheet, displayLogoResId, surveyInfoBarDelegate);
    }

    /**
     * Allows the survey infobar to be triggered when talkback is enabled.
     * @param view The view to attach the listener.
     * @param tab The tab to attach the infobar.
     */
    private void addAccessibilityClickListener(TextView view, Tab tab) {
        view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mClicked || !ChromeAccessibilityUtil.get().isAccessibilityEnabled()) return;
                showSurvey(tab);
                mClosedByInteraction = true;
            }
        });
    }

    @Override
    protected void onStartedHiding() {
        super.onStartedHiding();
        if (mClosedByInteraction) return;
        if (isFrontInfoBar()) {
            mDelegate.onSurveyInfoBarClosed(false, true);
        } else {
            mDelegate.onSurveyInfoBarClosed(false, false);
        }
    }

    /**
     * Shows the survey and closes the infobar.
     * @param tab The tab on which to show the survey.
     */
    private void showSurvey(Tab tab) {
        mClicked = true;
        mDelegate.onSurveyTriggered();

        // TODO(https://crbug.com/1212754): Move to ChromeSurveyController.
        SurveyController.getInstance().showSurveyIfAvailable(TabUtils.getActivity(tab), mSiteId,
                mShowAsBottomSheet, mDisplayLogoResId, mDelegate.getLifecycleDispatcher());
        super.onCloseButtonClicked();
    }

    @VisibleForTesting
    public NoUnderlineClickableSpan getClickableSpan() {
        return mClickableSpan;
    }

    @NativeMethods
    interface Natives {
        void create(WebContents webContents, String siteId, boolean showAsBottomSheet,
                int displayLogoResId, SurveyInfoBarDelegate surveyInfoBarDelegate);
        Tab getTab(long nativeSurveyInfoBar, SurveyInfoBar caller);
    }
}
