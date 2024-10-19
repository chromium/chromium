// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.util.Pair;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.text.SpanApplier;
import org.chromium.url.GURL;

import java.util.HashSet;

/** Preferences fragment to allow users to manage card benefits linked to their credit cards. */
public class AutofillCardBenefitsFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver,
                Preference.OnPreferenceClickListener,
                Preference.OnPreferenceChangeListener {
    public static final String CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION =
            "CardBenefits_LearnMoreLinkClicked";
    public static final String CARD_BENEFITS_TERMS_CLICKED_USER_ACTION =
            "CardBenefits_TermsLinkClicked";
    public static final String CARD_BENEFITS_TOGGLED_OFF_USER_ACTION = "CardBenefits_ToggledOff";
    public static final String CARD_BENEFITS_TOGGLED_ON_USER_ACTION = "CardBenefits_ToggledOn";
    public static final String PREF_LIST_TERMS_URL = "card_benefits_terms_url";
    public static final String LEARN_MORE_URL =
            "https://support.google.com/googlepay?p=card_benefits_chrome";

    // Preference keys for searching for specific preference in tests.
    @VisibleForTesting static final String PREF_KEY_ENABLE_CARD_BENEFIT = "enable_card_benefit";
    @VisibleForTesting static final String PREF_KEY_LEARN_ABOUT = "learn_about";
    @VisibleForTesting static final String PREF_KEY_CARD_BENEFIT_TERM = "card_benefit_term";

    private static Callback<Fragment> sObserverForTest;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private PersonalDataManager mPersonalDataManager;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.autofill_card_benefits_settings_page_title));

        // Create blank preference screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onResume() {
        super.onResume();
        // Rebuild the preference list in case any of the underlying data has been updated and if
        // any preferences need to be added/removed based on that.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        createCardBenefitSwitch();
        createLearnAboutCardBenefitsLink();
        createPreferencesForCardBenefitTerms();
        drawBottomDivider();
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @VisibleForTesting
    static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    private void createCardBenefitSwitch() {
        ChromeSwitchPreference cardBenefitSwitch = new ChromeSwitchPreference(getStyledContext());
        cardBenefitSwitch.setTitle(R.string.autofill_settings_page_card_benefits_label);
        cardBenefitSwitch.setSummary(R.string.autofill_settings_page_card_benefits_toggle_summary);
        cardBenefitSwitch.setKey(PREF_KEY_ENABLE_CARD_BENEFIT);
        cardBenefitSwitch.setChecked(mPersonalDataManager.isCardBenefitEnabled());
        cardBenefitSwitch.setOnPreferenceChangeListener(this);
        getPreferenceScreen().addPreference(cardBenefitSwitch);
    }

    private void createLearnAboutCardBenefitsLink() {
        TextMessagePreference learnAboutLinkPreference =
                new TextMessagePreference(getStyledContext(), /* attrs= */ null);
        learnAboutLinkPreference.setSummary(
                SpanApplier.applySpans(
                        getString(
                                R.string
                                        .autofill_settings_page_card_benefits_learn_about_link_text),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        openUrlInCct(LEARN_MORE_URL);
                                        RecordUserAction.record(
                                                CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION);
                                    }
                                })));
        learnAboutLinkPreference.setDividerAllowedAbove(false);
        learnAboutLinkPreference.setDividerAllowedBelow(false);
        learnAboutLinkPreference.setKey(PREF_KEY_LEARN_ABOUT);
        getPreferenceScreen().addPreference(learnAboutLinkPreference);
    }

    private void createPreferencesForCardBenefitTerms() {
        HashSet<Pair<String, String>> issuersAndProductDescriptions = new HashSet<>();

        // List the card for product terms redirect if:
        // 1. The card has a valid product term url.
        // 2. Same issuer and card product combination is not listed before.
        for (CreditCard card : mPersonalDataManager.getCreditCardsForSettings()) {
            Pair<String, String> issuerAndProductDescriptionPair =
                    Pair.create(card.getIssuerId(), card.getProductDescription());

            if (issuersAndProductDescriptions.contains(issuerAndProductDescriptionPair)
                    || GURL.isEmptyOrInvalid(card.getProductTermsUrl())) {
                continue;
            }

            issuersAndProductDescriptions.add(issuerAndProductDescriptionPair);

            // Add a preference for the credit card.
            ChromeBasePreference cardPref = new ChromeBasePreference(getStyledContext());
            cardPref.setDividerAllowedAbove(false);
            cardPref.setDividerAllowedBelow(false);
            cardPref.setTitle(issuerAndProductDescriptionPair.second);
            cardPref.setSummary(R.string.autofill_settings_page_card_benefits_issuer_term_text);
            cardPref.setKey(PREF_KEY_CARD_BENEFIT_TERM);

            // Add issuer site redirect.
            Bundle args = cardPref.getExtras();
            args.putString(PREF_LIST_TERMS_URL, card.getProductTermsUrl().getSpec());
            cardPref.setOnPreferenceClickListener(this);

            // Set card icon. It can be either a custom card art or a network icon.
            cardPref.setIcon(
                    AutofillUiUtils.getCardIcon(
                            getStyledContext(),
                            mPersonalDataManager,
                            card.getCardArtUrl(),
                            card.getIssuerIconDrawableId(),
                            ImageSize.LARGE,
                            ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)));

            getPreferenceScreen().addPreference(cardPref);
        }
    }

    private void drawBottomDivider() {
        RecyclerView recyclerView = getListView();
        recyclerView.addItemDecoration(new BottomDividerItemDecoration(getContext()));
    }

    private void openUrlInCct(String url) {
        new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(getContext(), Uri.parse(url));
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        openUrlInCct(preference.getExtras().getString(PREF_LIST_TERMS_URL));
        RecordUserAction.record(CARD_BENEFITS_TERMS_CLICKED_USER_ACTION);
        return true;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        boolean prefEnabled = (boolean) newValue;
        mPersonalDataManager.setCardBenefit(prefEnabled);
        RecordUserAction.record(
                prefEnabled
                        ? CARD_BENEFITS_TOGGLED_ON_USER_ACTION
                        : CARD_BENEFITS_TOGGLED_OFF_USER_ACTION);
        return true;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }

    // Custom ItemDecoration class that adds a divider at the end of the list.
    private static class BottomDividerItemDecoration extends RecyclerView.ItemDecoration {
        private static final int[] ATTRS = new int[] {android.R.attr.listDivider};
        private final Drawable mDivider;

        public BottomDividerItemDecoration(Context context) {
            final TypedArray a = context.obtainStyledAttributes(ATTRS);
            mDivider = a.getDrawable(0);
            a.recycle();
        }

        @Override
        public void onDrawOver(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
            int lastChildIndex = parent.getChildCount() - 1;
            if (lastChildIndex < 0) {
                return;
            }

            int left = parent.getPaddingLeft();
            int right = parent.getWidth() - parent.getPaddingRight();

            final View lastChild = parent.getChildAt(lastChildIndex);
            final Rect bounds = new Rect();
            parent.getDecoratedBoundsWithMargins(lastChild, bounds);
            final int bottom = bounds.bottom + Math.round(lastChild.getTranslationY());
            final int top = bottom - mDivider.getIntrinsicHeight();

            mDivider.setBounds(left, top, right, bottom);
            mDivider.draw(canvas);
        }
    }
}
