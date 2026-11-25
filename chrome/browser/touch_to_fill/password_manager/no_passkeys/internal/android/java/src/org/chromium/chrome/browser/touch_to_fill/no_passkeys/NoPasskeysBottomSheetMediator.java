// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.StyleSpan;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.adaptive_ui.DismissReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Mediator class for the no passkeys bottom sheet. */
@NullMarked
class NoPasskeysBottomSheetMediator implements NoPasskeysBottomSheetContent.Delegate {
    private final WeakReference<@Nullable BottomSheetController> mBottomSheetController;
    private final WeakReference<@Nullable AdaptiveUiController> mAdaptiveUiController;

    private @Nullable BottomSheetContent mBottomSheetContent;
    private @Nullable AdaptiveUiEntry mAdaptiveUiEntry;
    private NoPasskeysBottomSheetCoordinator.@Nullable NativeDelegate mNativeDelegate;

    /**
     * Creates the mediator.
     *
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param adaptiveUiController The {@link AdaptiveUiController} to use when enabled.
     * @param nativeDelegate A NoPasskeysBottomSheetBridge to interact with the native side.
     */
    NoPasskeysBottomSheetMediator(
            WeakReference<@Nullable BottomSheetController> bottomSheetController,
            WeakReference<@Nullable AdaptiveUiController> adaptiveUiController,
            NoPasskeysBottomSheetCoordinator.NativeDelegate nativeDelegate) {
        mBottomSheetController = bottomSheetController;
        mAdaptiveUiController = adaptiveUiController;
        mNativeDelegate = nativeDelegate;
    }

    /**
     * Requests to show the no passkeys surface. If the environment prevents showing, the method
     * will return early (e.g. because the finishing activity cleans up UI components).
     *
     * @param context The {@link Context} used to build the UI.
     * @param origin The formatted origin to display.
     * @return True if the surface was shown, or is taking over for the current content.
     */
    boolean show(Context context, String origin) {
        AdaptiveUiController adaptiveUiController = mAdaptiveUiController.get();
        if (adaptiveUiController != null) {
            mAdaptiveUiEntry = buildAdaptiveUiEntry(context, origin);
            // Like the legacy sheet, give up instead of waiting in a queue: a queued sheet that
            // BottomSheetController#clearRequestsAndHide() drops isn't always dismissed, which
            // would leave native waiting for onDismissed().
            return adaptiveUiController.show(mAdaptiveUiEntry) != null;
        }

        // TODO(crbug.com/581058945): Remove the legacy BottomSheetController fallback (and its
        // plumbing and tests) once AndroidAdaptiveUiTransitions launches.
        BottomSheetController bottomSheetController = mBottomSheetController.get();
        if (bottomSheetController == null) {
            return false;
        }
        mBottomSheetContent =
                new NoPasskeysBottomSheetContent(
                        context, createSubtitleText(context, origin), this);
        return bottomSheetController.requestShowContent(mBottomSheetContent, true);
    }

    private AdaptiveUiEntry buildAdaptiveUiEntry(Context context, String origin) {
        PropertyModel model =
                new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS)
                        .with(
                                AdaptiveUiProperties.DELEGATE,
                                new AdaptiveUiProperties.Delegate() {
                                    @Override
                                    public void onSecondaryButtonClicked() {
                                        notifyUseAnotherDevice();
                                    }

                                    @Override
                                    public void onDismissed(@DismissReason int reason) {
                                        mAdaptiveUiEntry = null;
                                        notifyNativeDismissed();
                                    }
                                })
                        .with(AdaptiveUiProperties.HEADER_ICON, createHeaderIcon(context))
                        .with(
                                AdaptiveUiProperties.TITLE,
                                context.getString(R.string.no_passkeys_sheet_title))
                        .with(AdaptiveUiProperties.DESCRIPTION, createSubtitleText(context, origin))
                        .with(
                                AdaptiveUiProperties.PRIMARY_BUTTON_TEXT,
                                context.getString(R.string.no_passkeys_sheet_ok))
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_TEXT,
                                context.getString(R.string.no_passkeys_sheet_use_another_device))
                        .with(
                                AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                R.string.no_passkeys_sheet_full_height)
                        .with(
                                AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                R.string.no_passkeys_sheet_closed)
                        .with(
                                AdaptiveUiProperties.SHEET_USER_CRITICAL,
                                UserCriticalFeature.TOUCH_TO_FILL_NO_PASSKEYS)
                        .build();
        return new AdaptiveUiEntry.Builder(model)
                .setScope(AdaptiveUiScope.TRANSIENT)
                .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                .addAllowedContainer(AdaptiveUiContainer.MODAL_DIALOG)
                .build();
    }

    /** Hide the bottom sheet (if showing) and ensures the dismiss callback is called. */
    void destroy() {
        dismiss();
        mBottomSheetContent = null;
        mAdaptiveUiEntry = null;
    }

    // NoPasskeysBottomSheetContent.Delegate implementation follows:
    @Override
    public void onClickOk() {
        dismiss();
    }

    @Override
    public void onClickUseAnotherDevice() {
        notifyUseAnotherDevice();
        dismiss();
    }

    @Override
    public void onDestroy() {
        destroy();
    }

    private void notifyUseAnotherDevice() {
        if (mNativeDelegate == null) {
            return;
        }
        mNativeDelegate.onClickUseAnotherDevice();
    }

    private void notifyNativeDismissed() {
        if (mNativeDelegate == null) {
            return; // Dismissal already happened.
        }
        mNativeDelegate.onDismissed();
        mNativeDelegate = null; // Don't call the callback repeatedly!
    }

    private void dismiss() {
        notifyNativeDismissed();
        if (mAdaptiveUiEntry != null) {
            AdaptiveUiEntry entry = mAdaptiveUiEntry;
            mAdaptiveUiEntry = null;
            AdaptiveUiController adaptiveUiController = mAdaptiveUiController.get();
            if (adaptiveUiController != null) {
                adaptiveUiController.dismiss(entry, DismissReason.DISMISSED_BY_FEATURE);
            }
        } else if (mBottomSheetContent != null) {
            BottomSheetController bottomSheetController = mBottomSheetController.get();
            assumeNonNull(bottomSheetController);
            bottomSheetController.hideContent(mBottomSheetContent, true);
        }
    }

    /** Builds the subtitle text with {@code origin} emphasized in bold. */
    private static SpannableString createSubtitleText(Context context, String origin) {
        String subtitleString = context.getString(R.string.no_passkeys_sheet_subtitle, origin);
        SpannableString spannable = new SpannableString(subtitleString);
        int startIndex = subtitleString.indexOf(origin);
        spannable.setSpan(
                new StyleSpan(Typeface.BOLD),
                startIndex,
                startIndex + origin.length(),
                Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        return spannable;
    }

    /**
     * Returns the header icon with its own drawable state, so another surface drawing the same
     * resource (such as at a different size) can't change how this one renders.
     */
    private static @Nullable Drawable createHeaderIcon(Context context) {
        Drawable icon =
                AppCompatResources.getDrawable(
                        context,
                        PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon());
        return icon == null ? null : icon.mutate();
    }
}
