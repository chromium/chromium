// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertThat;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ChipView;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the address accessory sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AddressAccessorySheetViewTest {
    private final AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccessorySheetCoordinator accessorySheet =
                    new AccessorySheetCoordinator(mActivityTestRule.getActivity().findViewById(
                            R.id.keyboard_accessory_sheet_stub));
            accessorySheet.setTabs(new KeyboardAccessoryData.Tab[] {new KeyboardAccessoryData.Tab(
                    "Addresses", null, null, R.layout.address_accessory_sheet,
                    AccessoryTabType.ADDRESSES, new KeyboardAccessoryData.Tab.Listener() {
                        @Override
                        public void onTabCreated(ViewGroup view) {
                            mView.set((RecyclerView) view);
                            AccessorySheetTabViewBinder.initializeView(mView.get(), null);
                            AddressAccessorySheetViewBinder.initializeView(mView.get(), mModel);
                        }

                        @Override
                        public void onTabShown() {}
                    })});
            accessorySheet.setHeight(
                    mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                            R.dimen.keyboard_accessory_sheet_height));
            accessorySheet.show();
        });
        CriteriaHelper.pollUiThread(Criteria.equals(true, () -> mView.get() != null));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingCaptionsToTheModelRendersThem() {
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(
                    new AccessorySheetDataPiece("Addresses", AccessorySheetDataPiece.Type.TITLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        View title = mView.get().findViewById(R.id.tab_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Addresses"));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoToTheModelRendersClickableActions() throws ExecutionException {
        final AtomicBoolean clicked = new AtomicBoolean();
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(
                    createInfo(
                            /*nameFull=*/"Maya J. Park",
                            /*companyName=*/"",
                            /*addressHomeLine1=*/"100 Test Str.",
                            /*addressHomeLine2=*/"",
                            /*addressHomeZip=*/"14482",
                            /*addressHomeCity=*/"Potsdam",
                            /*addressHomeState=*/"CO",
                            /*addressHomeCountry=*/"France",
                            /*phoneHomeWholeNumber=*/"+4917802048383",
                            /*emailAddress=*/"maya.park@googlemail.com", clicked),
                    AccessorySheetDataPiece.Type.ADDRESS_INFO));
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand("Manage addresses", null),
                    AccessorySheetDataPiece.Type.FOOTER_COMMAND));
        });

        // Wait until at least one element is rendered. Test devices with small screens will cause
        // the footer to not be created. Instantiating a footer still covers potential crashes.
        CriteriaHelper.pollUiThread(() -> mView.get().getChildCount() > 0);

        // Check that the titles are correct:
        assertThat(getChipText(R.id.name_full), is("Maya J. Park"));
        assertThat(getChipText(R.id.company_name), is(""));
        assertThat(getChipText(R.id.address_home_line_1), is("100 Test Str."));
        assertThat(getChipText(R.id.address_home_line_2), is(""));
        assertThat(getChipText(R.id.address_home_zip), is("14482"));
        assertThat(getChipText(R.id.address_home_city), is("Potsdam"));
        assertThat(getChipText(R.id.address_home_state), is("CO"));
        assertThat(getChipText(R.id.address_home_country), is("France"));
        assertThat(getChipText(R.id.phone_home_whole_number), is("+4917802048383"));
        assertThat(getChipText(R.id.email_address), is("maya.park@googlemail.com"));

        // Chips without content shouldn't be visible:
        assertThat(findChipView(R.id.company_name).isShown(), is(false));

        // Chips are clickable:
        TestThreadUtils.runOnUiThreadBlocking(findChipView(R.id.name_full)::performClick);
        assertThat(clicked.get(), is(true));
        clicked.set(false);
        TestThreadUtils.runOnUiThreadBlocking(findChipView(R.id.email_address)::performClick);
        assertThat(clicked.get(), is(true));
    }

    private UserInfo createInfo(String nameFull, String companyName, String addressHomeLine1,
            String addressHomeLine2, String addressHomeZip, String addressHomeCity,
            String addressHomeState, String addressHomeCountry, String phoneHomeWholeNumber,
            String emailAddress, AtomicBoolean clickRecorder) {
        UserInfo info = new UserInfo("", null);
        info.addField(
                new UserInfoField(nameFull, nameFull, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                companyName, companyName, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                addressHomeLine1, addressHomeLine1, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                addressHomeLine2, addressHomeLine2, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                addressHomeZip, addressHomeZip, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                addressHomeCity, addressHomeCity, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                addressHomeState, addressHomeState, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(addressHomeCountry, addressHomeCountry, "", false,
                item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(phoneHomeWholeNumber, phoneHomeWholeNumber, "", false,
                item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(
                emailAddress, emailAddress, "", false, item -> clickRecorder.set(true)));
        return info;
    }

    private ChipView findChipView(@IdRes int id) {
        assertThat(mView.get().getChildAt(0), instanceOf(LinearLayout.class));
        LinearLayout layout = (LinearLayout) mView.get().getChildAt(0);
        View view = layout.findViewById(id);
        assertThat(view, is(not(nullValue())));
        assertThat(view, instanceOf(ChipView.class));
        return ((ChipView) view);
    }

    private CharSequence getChipText(@IdRes int id) {
        return findChipView(id).getPrimaryTextView().getText();
    }
}
