// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.helper.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AddressAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * These tests render screenshots of various accessory sheets and compare them to a gold standard.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    private FrameLayout mContentView;
    private PropertyModel mSheetModel;

    // No @Rule since we only need the launching helpers. Adding the rule to the chain breaks with
    // any ParameterizedRunnerDelegate.
    private BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_AUTOFILL)
                    .build();

    public AccessorySheetRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        Map<String, Boolean> featureMap = new HashMap<>();
        featureMap.put(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES, false);
        FeatureList.setTestFeatures(featureMap);

        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    private static class TestFaviconHelper extends FaviconHelper {
        public TestFaviconHelper(Context context) {
            super(context);
        }

        @Override
        public void fetchFavicon(String origin, Callback<Drawable> setIconCallback) {
            setIconCallback.onResult(getDefaultIcon(origin));
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        FaviconHelper.setCreationStrategy(TestFaviconHelper::new);
        mActivityTestRule.launchActivity(null);
        // Calling #setTheme() explicitly because the test rule doesn't have the @Rule annotation
        // and won't apply the theme.
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AsyncViewStub sheetStub = initializeContentViewWithSheetStub();

            mSheetModel = createSheetModel(
                    mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                            R.dimen.keyboard_accessory_sheet_height));

            LazyConstructionPropertyMcp.create(mSheetModel, VISIBLE,
                    AsyncViewProvider.of(sheetStub, R.id.keyboard_accessory_sheet_container),
                    AccessorySheetViewBinder::bind);
        });
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        setRtlForTesting(false);
        try {
            ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAddingPasswordTabToModelRendersTabsView() throws Exception {
        final KeyboardAccessoryData.AccessorySheetData sheet =
                new KeyboardAccessoryData.AccessorySheetData(
                        AccessoryTabType.PASSWORDS, "Passwords", "");
        sheet.getUserInfoList().add(
                new KeyboardAccessoryData.UserInfo("http://psl.origin.com/", true));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("No username", "No username", "", false, null));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("Password", "Password for No username", "", true, cb -> {}));
        sheet.getFooterCommands().add(
                new KeyboardAccessoryData.FooterCommand("Suggest strong password", cb -> {}));
        sheet.getFooterCommands().add(
                new KeyboardAccessoryData.FooterCommand("Manage Passwords", cb -> {}));

        PasswordAccessorySheetCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                () -> new PasswordAccessorySheetCoordinator(mActivityTestRule.getActivity(), null));
        showSheetTab(coordinator, sheet);

        mRenderTestRule.render(mContentView, "Passwords");
    }

    // Tests rendering of Payments tab with both credit cards and promo code offers.
    // Promo code offers should appear above the credit cards.
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAddingCreditCardAndPromoCodeToModelRendersTabsView() throws Exception {
        final KeyboardAccessoryData.AccessorySheetData sheet =
                new KeyboardAccessoryData.AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS, "Payments", "");
        sheet.getUserInfoList().add(new KeyboardAccessoryData.UserInfo("", true));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("**** 9219", "Card for Todd Tester", "1", false, result -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("10", "10", "-1", false, result -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("2021", "2021", "-1", false, result -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("Todd Tester", "Todd Tester", "0", false, result -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("123", "123", "-1", false, result -> {}));
        sheet.getPromoCodeInfoList().add(new KeyboardAccessoryData.PromoCodeInfo());
        sheet.getPromoCodeInfoList().get(0).setPromoCode(new UserInfoField(
                "50$OFF", "Promo Code for Todd Tester", "1", false, result -> {}));
        sheet.getPromoCodeInfoList().get(0).setDetailsText(
                "Get $50 off when you use this code at checkout.");
        sheet.getFooterCommands().add(
                new KeyboardAccessoryData.FooterCommand("Manage payment methods", cb -> {}));

        CreditCardAccessorySheetCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new CreditCardAccessorySheetCoordinator(
                                mActivityTestRule.getActivity(), null));
        showSheetTab(coordinator, sheet);

        mRenderTestRule.render(mContentView, "credit_cards_and_promo_codes");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAddingAddressToModelRendersTabsView() throws Exception {
        // Construct a sheet with a few data fields. Leave gaps so that the footer is visible in the
        // screenshot (but supply the fields itself since the field count should be fixed).
        final KeyboardAccessoryData.AccessorySheetData sheet =
                new KeyboardAccessoryData.AccessorySheetData(
                        AccessoryTabType.ADDRESSES, "Addresses", "");
        sheet.getUserInfoList().add(new KeyboardAccessoryData.UserInfo("", true));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("Todd Tester", "Todd Tester", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField( // Unused company name field.
                new UserInfoField("", "", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("112 Second Str", "112 Second Str", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField( // Unused address line 2 field.
                new UserInfoField("", "", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField( // Unused ZIP code field.
                new UserInfoField("", "", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("Budatest", "Budatest", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField( // Unused state field.
                new UserInfoField("", "", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField( // Unused country field.
                new UserInfoField("", "", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField(
                new UserInfoField("+088343188321", "+088343188321", "", false, item -> {}));
        sheet.getUserInfoList().get(0).addField(new UserInfoField(
                "todd.tester@gmail.com", "todd.tester@gmail.com", "", false, item -> {}));
        sheet.getFooterCommands().add(
                new KeyboardAccessoryData.FooterCommand("Manage addresses", cb -> {}));

        AddressAccessorySheetCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AddressAccessorySheetCoordinator(mActivityTestRule.getActivity(), null));
        showSheetTab(coordinator, sheet);

        mRenderTestRule.render(mContentView, "Addresses");
    }

    private AsyncViewStub initializeContentViewWithSheetStub() {
        mContentView = (FrameLayout) LayoutInflater.from(mActivityTestRule.getActivity())
                               .inflate(R.layout.test_main, null);
        AsyncViewStub sheetStub = mContentView.findViewById(R.id.keyboard_accessory_sheet_stub);
        sheetStub.setLayoutResource(R.layout.keyboard_accessory_sheet);
        sheetStub.setShouldInflateOnBackgroundThread(true);
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(MATCH_PARENT,
                mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                        R.dimen.keyboard_accessory_sheet_height));
        layoutParams.gravity = Gravity.START | Gravity.BOTTOM;
        sheetStub.setLayoutParams(layoutParams);

        mActivityTestRule.getActivity().setContentView(mContentView);
        return sheetStub;
    }

    private static PropertyModel createSheetModel(int height) {
        return new PropertyModel
                .Builder(TABS, ACTIVE_TAB_INDEX, VISIBLE, HEIGHT, TOP_SHADOW_VISIBLE)
                .with(HEIGHT, height)
                .with(TABS, new ListModel<>())
                .with(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB)
                .with(VISIBLE, false)
                .with(TOP_SHADOW_VISIBLE, false)
                .build();
    }

    private void showSheetTab(AccessorySheetTabCoordinator sheetComponent,
            KeyboardAccessoryData.AccessorySheetData sheetData) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetModel.get(TABS).add(sheetComponent.getTab());
            Provider<KeyboardAccessoryData.AccessorySheetData> provider = new PropertyProvider<>();
            sheetComponent.registerDataProvider(provider);
            provider.notifyObservers(sheetData);
            mSheetModel.set(ACTIVE_TAB_INDEX, 0);
            mSheetModel.set(VISIBLE, true);
        });
        ViewUtils.waitForView(mContentView, withId(R.id.keyboard_accessory_sheet_frame));
    }
}
