// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.net.Uri;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.AutofillSuggestion.Payload;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** JNI wrapper for C++ TouchToFillPaymentMethodViewImpl. Delegates calls from native to Java. */
@JNINamespace("autofill")
@NullMarked
class TouchToFillPaymentMethodViewBridge {
    private final TouchToFillPaymentMethodComponent mComponent;
    private final Context mContext;

    private TouchToFillPaymentMethodViewBridge(
            TouchToFillPaymentMethodComponent.Delegate delegate,
            Context context,
            AutofillImageFetcher imageFetcher,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid) {
        mComponent = new TouchToFillPaymentMethodCoordinator();
        mComponent.initialize(
                context,
                imageFetcher,
                bottomSheetController,
                delegate,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
        mContext = context;
    }

    @CalledByNative
    private static @Nullable TouchToFillPaymentMethodViewBridge create(
            TouchToFillPaymentMethodComponent.Delegate delegate,
            Profile profile,
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillPaymentMethodViewBridge(
                delegate,
                context,
                AutofillImageFetcherFactory.getForProfile(profile),
                bottomSheetController,
                windowAndroid);
    }

    @CalledByNative
    private void showPaymentMethods(
            @JniType("std::vector") Object[] suggestions, boolean shouldShowScanCreditCard) {
        mComponent.showPaymentMethods(
                (List<AutofillSuggestion>) (List<?>) Arrays.asList(suggestions),
                shouldShowScanCreditCard);
    }

    @CalledByNative
    private void showIbans(@JniType("std::vector") List<PersonalDataManager.Iban> ibans) {
        mComponent.showIbans(ibans);
    }

    @CalledByNative
    private void showLoyaltyCards(
            @JniType("base::span<const LoyaltyCard>") List<LoyaltyCard> affiliatedLoyaltyCards,
            @JniType("base::span<const LoyaltyCard>") List<LoyaltyCard> allLoyaltyCards,
            boolean firstTimeUsage) {
        mComponent.showLoyaltyCards(affiliatedLoyaltyCards, allLoyaltyCards, firstTimeUsage);
    }

    @CalledByNative
    private void updateBnplPaymentMethod(
            @JniType("std::optional<int64_t>") @Nullable Long extractedAmount,
            boolean isAmountSupportedByAnyIssuer) {
        mComponent.updateBnplPaymentMethod(extractedAmount, isAmountSupportedByAnyIssuer);
    }

    @CalledByNative
    private void showProgressScreen() {
        mComponent.showProgressScreen();
    }

    @CalledByNative
    private void showBnplIssuers(
            @JniType("std::vector") List<BnplIssuerContext> bnplIssuerContexts, String footerText) {
        mComponent.showBnplIssuers(bnplIssuerContexts, footerText);
    }

    @CalledByNative
    private void showErrorScreen(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description) {
        mComponent.showErrorScreen(title, description);
    }

    @CalledByNative
    private void showBnplIssuerTos(BnplIssuerTosDetail bnplIssuerTosDetail) {
        mComponent.showBnplIssuerTos(bnplIssuerTosDetail);
    }

    @CalledByNative
    private void hideSheet() {
        mComponent.hideSheet();
    }

    @CalledByNative
    private static AutofillSuggestion createAutofillSuggestion(
            @JniType("std::u16string") String label,
            @JniType("std::u16string") String secondaryLabel,
            @JniType("std::u16string") String subLabel,
            @JniType("std::u16string") String secondarySubLabel,
            @SuggestionType int suggestionType,
            GURL customIconUrl,
            int iconId,
            boolean applyDeactivatedStyle,
            Payload payload) {
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSecondaryLabel(secondaryLabel)
                .setSubLabel(subLabel)
                .setSecondarySubLabel(secondarySubLabel)
                .setSuggestionType(suggestionType)
                .setCustomIconUrl(customIconUrl)
                .setIconId(iconId)
                .setApplyDeactivatedStyle(applyDeactivatedStyle)
                .setPayload(payload)
                .build();
    }

    @CalledByNative
    private SpannableString getSpannableString(
            String text, int spanStart, int spanEnd, String url) {
        SpannableString textWithLink = new SpannableString(text);
        textWithLink.setSpan(
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        openLink(url);
                    }
                },
                spanStart,
                spanEnd,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return textWithLink;
    }

    @CalledByNative
    private BnplIssuerTosDetail.LegalMessages convertLegalMessageLinesForBnplTos(
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines) {
        return new BnplIssuerTosDetail.LegalMessages(legalMessageLines, this::openLink);
    }

    private void openLink(String url) {
        assumeNonNull(mContext);
        new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(mContext, Uri.parse(url));
    }
}
