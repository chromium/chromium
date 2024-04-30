// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.components.ui_metrics.SadTabEvent;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ChromeBulletSpan;
import org.chromium.url.GURL;

/**
 * Represent the sad tab displayed in place of a crashed renderer. Instantiated on the first
 * |show()| request from a Tab, and destroyed together with it. TODO(crbug.com/40162422): Consider
 * moving this to its own target.
 */
public class SadTab extends EmptyTabObserver implements UserData, TabViewProvider {
    private static final Class<SadTab> USER_DATA_KEY = SadTab.class;

    private final Tab mTab;

    private View mView;

    /**
     * Counts the number of successive refreshes on the sad tab page. The count is is reset after a
     * successful page load.
     */
    private int mSadTabSuccessiveRefreshCounter;

    public static SadTab from(Tab tab) {
        SadTab sadTab = get(tab);
        if (sadTab == null) {
            sadTab = tab.getUserDataHost().setUserData(USER_DATA_KEY, new SadTab(tab));
        }
        return sadTab;
    }

    public static SadTab get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    public static boolean isShowing(Tab tab) {
        if (tab == null || !tab.isInitialized()) return false;
        SadTab sadTab = get(tab);
        return sadTab != null ? sadTab.isShowing() : false;
    }

    @VisibleForTesting
    public SadTab(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    /** Constructs and shows a sad tab (Aw, Snap!). */
    public void show(Context context, Runnable suggestionAction, Runnable buttonAction) {
        if (mTab.getWebContents() == null) return;

        if (mView != null) {
            assert !mTab.getTabViewManager().isShowing(this);
            // SadTab was requested to show but TabContentManager may have queued it in favor
            // of a tab view of other type such as |TabViewProvider.Type.SUSPENDED_TAB| or
            // |PAINT_PREVIEW}|. Just return and wait for the SadTab's turn to show up.
            return;
        }
        mSadTabSuccessiveRefreshCounter++;
        mView =
                createView(
                        context,
                        suggestionAction,
                        buttonAction,
                        showSendFeedbackView(),
                        mTab.isIncognito());

        mTab.getTabViewManager().addTabViewProvider(this);
    }

    /**
     * @return {@code true} if we should show 'Send Feedback'.
     */
    public boolean showSendFeedbackView() {
        // If the tab has crashed twice in a row change the sad tab view to the "Send Feedback"
        // version and change the onClickListener.
        return mSadTabSuccessiveRefreshCounter >= 2;
    }

    /** Removes the sad tab view if present. */
    @VisibleForTesting
    public void removeIfPresent() {
        mTab.getTabViewManager().removeTabViewProvider(this);
        mView = null;
    }

    /**
     * @return Whether or not the sad tab is showing.
     */
    public boolean isShowing() {
        return mView != null && mTab.getTabViewManager().isShowing(this);
    }

    // TabObserver

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
        removeIfPresent();
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        removeIfPresent();
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        // Reset the succressiveRefresh counter after successfully loading a page.
        mSadTabSuccessiveRefreshCounter = 0;
        removeIfPresent();
    }

    // UserData

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }

    @Override
    public @ColorInt int getBackgroundColor(Context context) {
        return context.getColor(R.color.baseline_neutral_90);
    }

    /**
     * @param context Context this sad tab is shown with.
     * @param suggestionAction {@link Runnable} to be executed when user clicks "try these
     *     suggestions".
     * @param buttonAction {@link Runnable} to be executed when the button is pressed. (e.g.,
     *     refreshing the page or sending feedback)
     * @param showSendFeedbackView Whether to show the "send feedback" version of the Sad Tab view.
     * @param isIncognito Whether the Sad Tab view is being showin in an incognito tab.
     * @return A {@link View} instance which is used in place of a crashed renderer.
     */
    protected View createView(
            Context context,
            final Runnable suggestionAction,
            Runnable buttonAction,
            boolean showSendFeedbackView,
            boolean isIncognito) {
        // Inflate Sad tab and initialize.
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View sadTabView = inflater.inflate(R.layout.sad_tab, null);
        sadTabView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        TextView titleText = (TextView) sadTabView.findViewById(R.id.sad_tab_title);
        int titleTextId =
                showSendFeedbackView ? R.string.sad_tab_reload_title : R.string.sad_tab_title;
        titleText.setText(titleTextId);

        if (showSendFeedbackView) intializeSuggestionsViews(context, sadTabView, isIncognito);

        TextView messageText = (TextView) sadTabView.findViewById(R.id.sad_tab_message);
        messageText.setText(getHelpMessage(context, suggestionAction, showSendFeedbackView));
        messageText.setMovementMethod(LinkMovementMethod.getInstance());

        Button button = (Button) sadTabView.findViewById(R.id.sad_tab_button);
        int buttonTextId =
                showSendFeedbackView
                        ? R.string.sad_tab_send_feedback_label
                        : R.string.sad_tab_reload_label;
        button.setText(buttonTextId);
        button.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        recordEvent(showSendFeedbackView, SadTabEvent.BUTTON_CLICKED);
                        buttonAction.run();
                    }
                });

        recordEvent(showSendFeedbackView, SadTabEvent.DISPLAYED);

        return sadTabView;
    }

    /**
     * Construct and return help message to be displayed on R.id.sad_tab_message.
     * @param context Context of the resulting Sad Tab view. This is needed to load the strings.
     * @param suggestionAction Action to be executed when user clicks "try these suggestions"
     *                         or "learn more".
     * @return Help message to be displayed on R.id.sad_tab_message.
     */
    private static CharSequence getHelpMessage(
            Context context, final Runnable suggestionAction, final boolean showSendFeedback) {
        NoUnderlineClickableSpan linkSpan =
                new NoUnderlineClickableSpan(
                        context,
                        (view) -> {
                            recordEvent(showSendFeedback, SadTabEvent.HELP_LINK_CLICKED);
                            suggestionAction.run();
                        });

        if (showSendFeedback) {
            SpannableString learnMoreLink =
                    new SpannableString(context.getString(R.string.sad_tab_reload_learn_more));
            learnMoreLink.setSpan(linkSpan, 0, learnMoreLink.length(), 0);
            return learnMoreLink;
        } else {
            String helpMessage =
                    context.getString(R.string.sad_tab_message)
                            + "\n\n"
                            + context.getString(R.string.sad_tab_suggestions);
            return SpanApplier.applySpans(helpMessage, new SpanInfo("<link>", "</link>", linkSpan));
        }
    }

    /**
     * Initializes the TextViews that display tips for handling repeated crashes.
     * @param context Context of the resulting Sad Tab view.
     * @param sadTabView The parent Sad Tab view that contains the TextViews.
     * @param isIncognito Whether the Sad Tab view is being showing in an incognito tab.
     */
    private static void intializeSuggestionsViews(
            Context context, View sadTabView, boolean isIncognito) {
        TextView suggestionsTitle =
                (TextView) sadTabView.findViewById(R.id.sad_tab_suggestions_title);
        suggestionsTitle.setVisibility(View.VISIBLE);
        suggestionsTitle.setText(R.string.sad_tab_reload_try);

        TextView suggestions = (TextView) sadTabView.findViewById(R.id.sad_tab_suggestions);
        suggestions.setVisibility(View.VISIBLE);

        SpannableStringBuilder spannableString = new SpannableStringBuilder();
        if (!isIncognito) {
            spannableString
                    .append(generateBulletedString(context, R.string.sad_tab_reload_incognito))
                    .append("\n");
        }
        spannableString
                .append(generateBulletedString(context, R.string.sad_tab_reload_restart_browser))
                .append("\n")
                .append(generateBulletedString(context, R.string.sad_tab_reload_restart_device))
                .append("\n");
        suggestions.setText(spannableString);
    }

    /**
     * Generates a bulleted {@link SpannableString}.
     * @param context The {@link Context} used to retrieve the String.
     * @param stringResId The resource id of the String to bullet.
     * @return A {@link SpannableString} with a bullet in front of the provided String.
     */
    private static SpannableString generateBulletedString(Context context, int stringResId) {
        SpannableString bullet = new SpannableString(context.getString(stringResId));
        bullet.setSpan(new ChromeBulletSpan(context), 0, bullet.length(), 0);
        return bullet;
    }

    /**
     * Records enumerated histograms for {@link SadTabEvent}.
     * @param sendFeedbackView Whether the event is for the "send feedback" version of the Sad Tab.
     * @param event The {@link SadTabEvent} to record.
     */
    private static void recordEvent(boolean sendFeedbackView, int event) {
        if (sendFeedbackView) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.SadTab.Feedback.Event", event, SadTabEvent.MAX_SAD_TAB_EVENT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.SadTab.Reload.Event", event, SadTabEvent.MAX_SAD_TAB_EVENT);
        }
    }

    // Bare minimum set up so |isShowing| returns true.
    public static void initForTesting(Tab tab, SadTab sadTab) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, sadTab);
    }

    @Override
    public int getTabViewProviderType() {
        return Type.SAD_TAB;
    }

    @Override
    public View getView() {
        return mView;
    }
}
