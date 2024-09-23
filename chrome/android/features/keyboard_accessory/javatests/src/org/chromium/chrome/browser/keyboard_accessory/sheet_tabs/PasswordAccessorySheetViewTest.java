// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.OptionToggle;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PasskeySection;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PlusAddressInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/** View tests for the password accessory sheet. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessorySheetViewTest {
    private AccessorySheetTabItemsModel mModel;
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = new AccessorySheetTabItemsModel();

                    AccessorySheetCoordinator accessorySheet =
                            new AccessorySheetCoordinator(
                                    mActivityTestRule
                                            .getActivity()
                                            .findViewById(R.id.keyboard_accessory_sheet_stub),
                                    null);
                    accessorySheet.setTabs(
                            new KeyboardAccessoryData.Tab[] {
                                new KeyboardAccessoryData.Tab(
                                        "Passwords",
                                        null,
                                        null,
                                        R.layout.password_accessory_sheet,
                                        AccessoryTabType.ALL,
                                        new KeyboardAccessoryData.Tab.Listener() {
                                            @Override
                                            public void onTabCreated(ViewGroup view) {
                                                mView.set((RecyclerView) view);
                                                AccessorySheetTabViewBinder.initializeView(
                                                        mView.get(), null);
                                                PasswordAccessorySheetViewBinder.UiConfiguration
                                                        uiConfiguration =
                                                                new PasswordAccessorySheetViewBinder
                                                                        .UiConfiguration();
                                                uiConfiguration.faviconHelper =
                                                        FaviconHelper.create(
                                                                view.getContext(),
                                                                mActivityTestRule.getProfile(
                                                                        false));
                                                PasswordAccessorySheetViewBinder.initializeView(
                                                        mView.get(),
                                                        PasswordAccessorySheetCoordinator
                                                                .createAdapter(
                                                                        uiConfiguration, mModel));
                                            }

                                            @Override
                                            public void onTabShown() {}
                                        })
                            });
                    accessorySheet.setHeight(
                            mActivityTestRule
                                    .getActivity()
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.keyboard_accessory_sheet_height));
                    accessorySheet.show();
                });
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get(), notNullValue()));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingCaptionsToTheModelRendersThem() {
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    "Passwords", AccessorySheetDataPiece.Type.TITLE));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));
        View title = mView.get().findViewById(R.id.tab_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Passwords"));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoToTheModelRendersClickableActions() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        UserInfo testInfo = new UserInfo("", false);
        testInfo.addField(
                new UserInfoField(
                        "Name Suggestion",
                        "Name Suggestion",
                        "",
                        false,
                        item -> clicked.set(true)));
        testInfo.addField(
                new UserInfoField(
                        "Password Suggestion",
                        "Password Suggestion",
                        "",
                        true,
                        item -> clicked.set(true)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    testInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));

        assertThat(getNameSuggestion().getPrimaryTextView().getText(), is("Name Suggestion"));
        assertThat(
                getPasswordSuggestion().getPrimaryTextView().getText(), is("Password Suggestion"));
        assertThat(
                getPasswordSuggestion().getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));

        ThreadUtils.runOnUiThreadBlocking(getNameSuggestion()::performClick);
        assertThat(clicked.get(), is(true));
        clicked.set(false);
        ThreadUtils.runOnUiThreadBlocking(getPasswordSuggestion()::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingPasskeySectionToTheModelRendersClickableActions()
            throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        final PasskeySection kTestPasskey =
                new PasskeySection("Passkey User", () -> clicked.set(true));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    kTestPasskey, AccessorySheetDataPiece.Type.PASSKEY_SECTION));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));

        assertThat(
                getPasskeyChipAt(0).getPrimaryTextView().getText(),
                is(kTestPasskey.getDisplayName()));
        assertThat(
                getPasskeyChipAt(0).getSecondaryTextView().getText(),
                is(getString(R.string.password_accessory_passkey_label)));

        ThreadUtils.runOnUiThreadBlocking(getPasskeyChipAt(0)::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingPlusAddressInfoToTheModelRendersClickableActions()
            throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    new PlusAddressInfo(
                                            /* origin= */ "google.com",
                                            new UserInfoField.Builder()
                                                    .setDisplayText("example@gmail.com")
                                                    .setTextToFill("example@gmail.com")
                                                    .setIsObfuscated(false)
                                                    .setCallback(unused -> clicked.set(true))
                                                    .build()),
                                    AccessorySheetDataPiece.Type.PLUS_ADDRESS_SECTION));
                });

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mView.get().getChildCount(), greaterThan(0)));

        assertThat(getPlusAddressChipAt(0).getPrimaryTextView().getText(), is("example@gmail.com"));

        // Plus address chip is clickable:
        ThreadUtils.runOnUiThreadBlocking(getPlusAddressChipAt(0)::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoWithObfuscatedTextAndNullCallbackRendersDialog()
            throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        UserInfo usernameEnabled = new UserInfo("", false);
        usernameEnabled.addField(
                new UserInfoField("username1", "username1", "", false, item -> clicked.set(true)));
        usernameEnabled.addField(
                new UserInfoField("pa55w0rd", "Password for username1", "", true, null));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    usernameEnabled, AccessorySheetDataPiece.Type.PASSWORD_INFO));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));

        assertThat(getNameSuggestion().getPrimaryTextView().getText(), is("username1"));
        assertThat(getPasswordSuggestion().getPrimaryTextView().getText(), is("pa55w0rd"));
        assertThat(
                getPasswordSuggestion().getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));

        ThreadUtils.runOnUiThreadBlocking(getNameSuggestion()::performClick);
        assertThat(clicked.get(), is(true));
        ThreadUtils.runOnUiThreadBlocking(getPasswordSuggestion()::performClick);
        assertInsecureFillingDialog();
    }

    @Test
    @MediumTest
    public void testAddingUserInfoTitlesAreRenderedIfNotEmpty() {
        assertThat(mView.get().getChildCount(), is(0));
        final UserInfoField kUnusedInfoField =
                new UserInfoField("Unused Name", "Unused Password", "", false, cb -> {});

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserInfo sameOriginInfo = new UserInfo("", true);
                    sameOriginInfo.addField(kUnusedInfoField);
                    sameOriginInfo.addField(kUnusedInfoField);
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    sameOriginInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));

                    UserInfo pslOriginInfo = new UserInfo("other.origin.eg", false);
                    pslOriginInfo.addField(kUnusedInfoField);
                    pslOriginInfo.addField(kUnusedInfoField);
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    pslOriginInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(2)));

        assertThat(getUserInfoAt(0).getTitle().isShown(), is(false));
        assertThat(getUserInfoAt(1).getTitle().isShown(), is(true));
        assertThat(getUserInfoAt(1).getTitle().getText(), is("other.origin.eg"));
    }

    @Test
    @MediumTest
    public void testOptionToggleRenderedIfNotEmpty() throws ExecutionException {
        assertThat(mView.get().getChildCount(), is(0));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OptionToggle toggle =
                            new OptionToggle(
                                    "Save passwords for this site",
                                    false,
                                    AccessoryAction.TOGGLE_SAVE_PASSWORDS,
                                    result -> {});
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));
        View title = mView.get().findViewById(R.id.option_toggle_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Save passwords for this site"));

        View subtitle = mView.get().findViewById(R.id.option_toggle_subtitle);
        assertThat(subtitle, is(not(nullValue())));
        assertThat(subtitle, instanceOf(TextView.class));
        assertThat(subtitle, withText(R.string.text_off));

        View switchView = mView.get().findViewById(R.id.option_toggle_switch);
        assertThat(switchView, is(not(nullValue())));
        assertThat(switchView, instanceOf(SwitchCompat.class));
        assertFalse(((SwitchCompat) switchView).isChecked());
    }

    @Test
    @MediumTest
    public void testClickingDisabledToggleInvokesCallbackToEnable() throws ExecutionException {
        AtomicReference<Boolean> toggleEnabled = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OptionToggle toggle =
                            new OptionToggle(
                                    "Save passwords for this site",
                                    false,
                                    AccessoryAction.TOGGLE_SAVE_PASSWORDS,
                                    toggleEnabled::set);
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));
        ThreadUtils.runOnUiThreadBlocking(
                mView.get().findViewById(R.id.option_toggle)::performClick);
        assertTrue(toggleEnabled.get());
    }

    @Test
    @MediumTest
    public void testClickingEnabledToggleInvokesCallbackToDisable() throws ExecutionException {
        AtomicReference<Boolean> toggleEnabled = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OptionToggle toggle =
                            new OptionToggle(
                                    "Save passwords for this site",
                                    true,
                                    AccessoryAction.TOGGLE_SAVE_PASSWORDS,
                                    toggleEnabled::set);
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));
        ThreadUtils.runOnUiThreadBlocking(
                mView.get().findViewById(R.id.option_toggle)::performClick);

        assertFalse(toggleEnabled.get());
    }

    private String getString(@StringRes int strId) {
        return mView.get().getResources().getString(strId);
    }

    private ChipView getPlusAddressChipAt(int index) {
        assertThat(mView.get().getChildCount(), is(greaterThan(index)));
        assertThat(mView.get().getChildAt(index), instanceOf(ViewGroup.class));
        LinearLayout plusAddressInfo = (LinearLayout) mView.get().getChildAt(index);
        return plusAddressInfo.findViewById(R.id.plus_address);
    }

    private ChipView getPasskeyChipAt(int index) {
        assertThat(mView.get().getChildCount(), is(greaterThan(index)));
        assertThat(mView.get().getChildAt(index), instanceOf(ViewGroup.class));
        LinearLayout passkeySection = (LinearLayout) mView.get().getChildAt(index);
        return passkeySection.findViewById(R.id.keyboard_accessory_sheet_chip);
    }

    private PasswordAccessoryInfoView getUserInfoAt(int index) {
        assertThat(mView.get().getChildCount(), is(greaterThan(index)));
        assertThat(mView.get().getChildAt(index), instanceOf(PasswordAccessoryInfoView.class));
        return (PasswordAccessoryInfoView) mView.get().getChildAt(index);
    }

    private ChipView getNameSuggestion() {
        View view = getUserInfoAt(0).findViewById(R.id.suggestion_text);
        assertThat(view, is(not(nullValue())));
        assertThat(view, instanceOf(ChipView.class));
        return (ChipView) view;
    }

    private ChipView getPasswordSuggestion() {
        View view = getUserInfoAt(0).findViewById(R.id.password_text);
        assertThat(view, is(not(nullValue())));
        assertThat(view, instanceOf(ChipView.class));
        return (ChipView) view;
    }

    private void assertInsecureFillingDialog() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    onView(withText(R.string.passwords_not_secure_filling))
                            .inRoot(isDialog())
                            .check(matches(isDisplayed()));
                    onView(withText(R.string.passwords_not_secure_filling_details))
                            .inRoot(isDialog())
                            .check(matches(isDisplayed()));
                });
    }
}
