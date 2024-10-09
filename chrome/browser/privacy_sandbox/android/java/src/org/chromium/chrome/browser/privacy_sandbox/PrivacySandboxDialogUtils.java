// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Privacy Sandbox Dialog utility class: collects some common UI utils for Notice and Consent
 * dialogs.
 */
public class PrivacySandboxDialogUtils {
    /**
     * Create an expand Drawable with its own animation.
     *
     * @return an expandable/collapsible Drawable icon.
     */
    public static Drawable createExpandDrawable(Context context) {
        StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
        StateListDrawableBuilder.State checked =
                builder.addState(
                        R.drawable.ic_expand_less_black_24dp, android.R.attr.state_checked);
        StateListDrawableBuilder.State unchecked =
                builder.addState(R.drawable.ic_expand_more_black_24dp);
        builder.addTransition(
                checked, unchecked, R.drawable.transition_expand_less_expand_more_black_24dp);
        builder.addTransition(
                unchecked, checked, R.drawable.transition_expand_more_expand_less_black_24dp);

        Drawable tintableDrawable = DrawableCompat.wrap(builder.build());
        DrawableCompat.setTintList(
                tintableDrawable,
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));
        return tintableDrawable;
    }

    /**
     * Set the correct content description for the dropdown element based on its expanded/collapsed
     * state, concatenating the dropdown text with its state. Used for accessibility purposes.
     *
     * @param context The current context.
     * @param dropdownElement The dropdown View element.
     * @param isDropdownExpanded boolean indicating if the dropdown status is expanded/collapses
     * @param stringRes Dropdown text resource.
     */
    public static void updateDropdownControlContentDescription(
            Context context,
            View dropdownElement,
            boolean isDropdownExpanded,
            @StringRes int stringRes) {
        String dropdownButtonText = context.getResources().getString(stringRes);

        String collapseOrExpandedText =
                context.getResources()
                        .getString(
                                isDropdownExpanded
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group);

        String description =
                context.getResources()
                        .getString(
                                R.string.concat_two_strings_with_periods,
                                dropdownButtonText,
                                collapseOrExpandedText);
        dropdownElement.setContentDescription(description);
    }

    /**
     * Creates a ChromeBulletSpan and set its text.
     *
     * @param context Current context.
     * @param targetLayout The layout view where the bullet should live.
     * @param bulletViewId The bullet viewId.
     * @param stringRes The string resource from where the text is retrieved.
     */
    public static void setBulletText(
            Context context, View targetLayout, @IdRes int bulletViewId, @StringRes int stringRes) {
        TextView bulletView = targetLayout.findViewById(bulletViewId);
        SpannableString bullet = new SpannableString(context.getResources().getString(stringRes));

        bullet.setSpan(new ChromeBulletSpan(context), 0, bullet.length(), 0);
        bulletView.setText(bullet);
    }

    /**
     * Creates a ChromeBulletSpan with bold content and set its text.
     *
     * @param context Current context.
     * @param targetLayout The layout view where the bullet should live.
     * @param bulletViewId The bullet viewId.
     * @param stringRes The string resource from where the text is retrieved.
     */
    public static void setBulletTextWithBoldContent(
            Context context,
            ViewGroup targetLayout,
            @IdRes int bulletViewId,
            @StringRes int stringRes) {
        TextView view = targetLayout.findViewById(bulletViewId);
        SpannableString spannableString =
                SpanApplier.applySpans(
                        context.getResources().getString(stringRes),
                        new SpanApplier.SpanInfo(
                                "<b>",
                                "</b>",
                                new TextAppearanceSpan(
                                        context,
                                        R.style.TextAppearance_TextMediumThick_Secondary)));
        spannableString.setSpan(new ChromeBulletSpan(context), 0, spannableString.length(), 0);
        view.setText(spannableString);
    }
}
