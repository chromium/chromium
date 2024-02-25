// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.ui.text.SpanApplier;

/**
 * This interface includes methods that are shared in LegacyIncognitoDescriptionView and
 * RevampedIncognitoDescriptionView.
 */
public interface IncognitoDescriptionView {
    static final String TRACKING_PROTECTION_URL =
            "https://support.google.com/chrome/?p=pause_protections";

    /**
     * Set learn more on click listener.
     * @param listener The given listener.
     */
    void setLearnMoreOnclickListener(View.OnClickListener listener);

    /**
     * Set cookie controls toggle's checked value.
     * @param enabled The value to set the toggle to.
     */
    void setCookieControlsToggle(boolean enabled);

    /**
     * Set cookie controls toggle on checked change listerner.
     * @param listener The given listener.
     */
    void setCookieControlsToggleOnCheckedChangeListener(
            CompoundButton.OnCheckedChangeListener listener);

    /**
     * Sets the cookie controls enforced state.
     * @param enforcement A CookieControlsEnforcement enum type indicating the type of
     *         enforcement policy being applied to Cookie Controls.
     */
    void setCookieControlsEnforcement(int enforcement);

    /**
     * Add click listener that redirects user to the Cookie Control Settings.
     * @param listener The given listener.
     */
    void setCookieControlsIconOnclickListener(View.OnClickListener listener);

    /** Formats the Tracking Protection card when it exists. */
    default void formatTrackingProtectionText(Context context, View layout) {
        TextView view = (TextView) layout.findViewById(R.id.tracking_protection_description_two);
        if (view == null) return;

        String text =
                context.getResources()
                        .getString(R.string.new_tab_otr_third_party_blocked_cookie_part_two);
        ClickableSpan span =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        new ChromeAsyncTabLauncher(/* incognito= */ true)
                                .launchUrl(TRACKING_PROTECTION_URL, TabLaunchType.FROM_CHROME_UI);
                    }

                    @Override
                    public void updateDrawState(TextPaint textPaint) {
                        super.updateDrawState(textPaint);
                        textPaint.setColor(
                                context.getColor(R.color.default_text_color_secondary_light_list));
                    }
                };
        view.setText(
                SpanApplier.applySpans(text, new SpanApplier.SpanInfo("<link>", "</link>", span)));
        view.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
