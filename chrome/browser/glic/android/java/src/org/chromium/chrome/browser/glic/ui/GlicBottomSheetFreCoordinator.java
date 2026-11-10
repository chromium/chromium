// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Typeface;
import android.text.method.LinkMovementMethod;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.R;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** Coordinates the first-run experience bottom sheet view for Glic. */
@NullMarked
class GlicBottomSheetFreCoordinator {
    private final Context mContext;
    private final Runnable mOnAccept;
    private final Runnable mOnDecline;
    private final SettingsCustomTabLauncher mCustomTabLauncher;
    private final View mRootView;
    private final ViewGroup mContentContainer;
    private final ViewGroup mSkeletonContainer;
    private final LottieAnimationView mHeaderAnimation;
    private final TextViewWithClickableSpans mBodyView;
    private final ButtonCompat mAcceptButton;
    private final ChromeImageButton mCloseButton;
    private @Nullable GlicBottomSheetFreTransitionController mFadeOut;
    private boolean mAccepted;
    private boolean mDeclined;
    private boolean mDestroyed;

    /** Creates coordinator with optional background color override (pass 0 for default). */
    GlicBottomSheetFreCoordinator(
            Context context,
            ViewGroup parent,
            boolean isEnterpriseAccount,
            @ColorInt int backgroundColor,
            SettingsCustomTabLauncher customTabLauncher,
            Runnable onAccept,
            Runnable onDecline) {
        mContext = context;
        mCustomTabLauncher = customTabLauncher;
        mOnAccept = onAccept;
        mOnDecline = onDecline;

        mRootView =
                LayoutInflater.from(context).inflate(R.layout.glic_fre_bottom_sheet, parent, false);
        int defaultBg = SemanticColorUtils.getColorSurfaceContainerHigh(context);
        mRootView.setBackgroundColor(backgroundColor == 0 ? defaultBg : backgroundColor);
        parent.addView(mRootView);

        mContentContainer = mRootView.findViewById(R.id.glic_fre_content_container);
        mSkeletonContainer = mRootView.findViewById(R.id.glic_fre_skeleton_container);
        mHeaderAnimation = mRootView.findViewById(R.id.glic_fre_header_animation);
        if (ValueAnimator.areAnimatorsEnabled()) {
            mHeaderAnimation.playAnimation();
        } else {
            mHeaderAnimation.setProgress(1.0f);
        }
        mBodyView = mRootView.findViewById(R.id.glic_fre_body);
        mAcceptButton = mRootView.findViewById(R.id.glic_fre_accept_button);
        mCloseButton = mRootView.findViewById(R.id.glic_fre_close_button);

        ViewCompat.setAccessibilityPaneTitle(
                mContentContainer, context.getString(R.string.glic_fre_dialog_title));

        mAcceptButton.setOnClickListener(v -> onClick(true));
        mCloseButton.setOnClickListener(v -> onClick(false));
        mBodyView.setMovementMethod(LinkMovementMethod.getInstance());
        updateBodyText(isEnterpriseAccount);
    }

    private void onClick(boolean accept) {
        if (mDestroyed || mDeclined || (accept && mAccepted)) return;
        mAccepted |= accept;
        mDeclined |= !accept;
        (accept ? mOnAccept : mOnDecline).run();
    }

    private SpanInfo link(String tag, @StringRes int urlRes) {
        return new SpanInfo(
                "<" + tag + ">",
                "</" + tag + ">",
                new ChromeClickableSpan(
                        mContext,
                        v -> {
                            if (!mDestroyed) {
                                mCustomTabLauncher.openUrlInCct(
                                        mContext, mContext.getString(urlRes));
                            }
                        }));
    }

    private static SpanInfo bold(String tag) {
        return new SpanInfo("<" + tag + ">", "</" + tag + ">", new StyleSpan(Typeface.BOLD));
    }

    private void updateBodyText(boolean isEnterpriseAccount) {
        @StringRes
        int descriptionRes =
                isEnterpriseAccount
                        ? R.string.glic_fre_dialog_enterprise_description_bold
                        : R.string.glic_fre_dialog_tab_sharing_description_bold;
        SpanInfo termsLink = link("link1", R.string.glic_fre_dialog_terms_url);
        SpanInfo privacyLink = link("link2", R.string.glic_fre_dialog_privacy_notice_url);
        SpanInfo[] spans =
                isEnterpriseAccount
                        ? new SpanInfo[] {termsLink, privacyLink, bold("b1"), bold("b2")}
                        : new SpanInfo[] {termsLink, privacyLink, bold("b")};
        String text =
                mContext.getString(
                        R.string.glic_fre_bottom_sheet_body, mContext.getString(descriptionRes));
        mBodyView.setText(SpanApplier.applySpans(text, spans));
    }

    View getView() {
        return mRootView;
    }

    /** Fades the card out, then runs {@code onDone}. {@link #destroy()} cancels the fade. */
    void fadeOut(Runnable onDone) {
        if (mDestroyed || mFadeOut != null) return;
        // TODO(b/578631452): Pass the WebUI load state or remove the skeleton path.
        GlicBottomSheetFreTransitionController fadeOut =
                new GlicBottomSheetFreTransitionController(
                        mRootView,
                        mContentContainer,
                        mSkeletonContainer,
                        /* isWebUiReady= */ true,
                        /* isWebUiLoadError= */ false,
                        (wasSkeletonShown, result) -> onDone.run());
        mFadeOut = fadeOut;
        fadeOut.onAccepted();
    }

    /** Removes the view and stops all callbacks. Safe to call more than once. */
    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        if (mFadeOut != null) mFadeOut.destroy();
        mHeaderAnimation.cancelAnimation();
        ViewGroup parent = (ViewGroup) mRootView.getParent();
        if (parent != null) parent.removeView(mRootView);
    }
}
