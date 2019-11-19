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
 * View tests for the credit card accessory sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CreditCardAccessorySheetViewTest {
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
                    "Credit Cards", null, null, R.layout.credit_card_accessory_sheet,
                    AccessoryTabType.CREDIT_CARDS, new KeyboardAccessoryData.Tab.Listener() {
                        @Override
                        public void onTabCreated(ViewGroup view) {
                            mView.set((RecyclerView) view);
                            AccessorySheetTabViewBinder.initializeView(mView.get(), null);
                            CreditCardAccessorySheetViewBinder.initializeView(mView.get(), mModel);
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
            mModel.add(new AccessorySheetDataPiece(
                    "Credit Cards", AccessorySheetDataPiece.Type.TITLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        View title = mView.get().findViewById(R.id.tab_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Credit Cards"));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoToTheModelRendersClickableActions() throws ExecutionException {
        final AtomicBoolean clicked = new AtomicBoolean();
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(
                    createInfo("4111111111111111", "04", "2034", "Kirby Puckett", clicked),
                    AccessorySheetDataPiece.Type.CREDIT_CARD_INFO));
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand("Manage credit cards", null),
                    AccessorySheetDataPiece.Type.FOOTER_COMMAND));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, () -> mView.get().getChildCount()));

        // Check that the titles are correct:
        assertThat(getChipText(R.id.cc_number), is("4111111111111111"));
        assertThat(getChipText(R.id.exp_month), is("04"));
        assertThat(getChipText(R.id.exp_year), is("2034"));
        assertThat(getChipText(R.id.cardholder), is("Kirby Puckett"));

        // Chips are clickable:
        TestThreadUtils.runOnUiThreadBlocking(findChipView(R.id.cc_number)::performClick);
        assertThat(clicked.get(), is(true));
        clicked.set(false);
        TestThreadUtils.runOnUiThreadBlocking(findChipView(R.id.exp_month)::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingUnselectableFieldsRendersUnclickabeChips() {
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserInfo infoWithUnclickableField = new UserInfo("", null);
            infoWithUnclickableField.addField(
                    new UserInfoField("4111111111111111", "4111111111111111", "", false, null));
            infoWithUnclickableField.addField(new UserInfoField("", "", "month", false, null));
            infoWithUnclickableField.addField(new UserInfoField("", "", "year", false, null));
            infoWithUnclickableField.addField(new UserInfoField("", "", "name", false, null));
            mModel.add(new AccessorySheetDataPiece(
                    infoWithUnclickableField, AccessorySheetDataPiece.Type.CREDIT_CARD_INFO));
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand("Manage credit cards", null),
                    AccessorySheetDataPiece.Type.FOOTER_COMMAND));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, () -> mView.get().getChildCount()));

        assertThat(getChipText(R.id.cc_number), is("4111111111111111"));
        assertThat(findChipView(R.id.cc_number).isShown(), is(true));
        assertThat(findChipView(R.id.cc_number).isEnabled(), is(false));
    }

    @Test
    @MediumTest
    public void testEmptyChipsAreNotVisible() {
        final AtomicBoolean clicked = new AtomicBoolean();
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(
                    // Cardholder name is empty
                    createInfo("4111111111111111", "04", "2034", "", clicked),
                    AccessorySheetDataPiece.Type.CREDIT_CARD_INFO));
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand("Manage credit cards", null),
                    AccessorySheetDataPiece.Type.FOOTER_COMMAND));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, () -> mView.get().getChildCount()));

        assertThat(findChipView(R.id.cardholder).isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testRendersWarning() {
        final String kWarning = "Insecure, so filling is no.";
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(kWarning, AccessorySheetDataPiece.Type.WARNING));
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand("Manage credit cards", null),
                    AccessorySheetDataPiece.Type.FOOTER_COMMAND));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, () -> mView.get().getChildCount()));

        assertThat(mView.get().getChildAt(0), instanceOf(LinearLayout.class));
        LinearLayout warning = (LinearLayout) mView.get().getChildAt(0);
        assertThat(warning.findViewById(R.id.tab_title), instanceOf(TextView.class));
        TextView warningText = warning.findViewById(R.id.tab_title);
        assertThat(warningText.isShown(), is(true));
        assertThat(warningText.getText(), is(kWarning));
    }

    private UserInfo createInfo(
            String number, String month, String year, String name, AtomicBoolean clickRecorder) {
        UserInfo info = new UserInfo("", null);
        info.addField(
                new UserInfoField(number, number, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(month, month, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(year, year, "", false, item -> clickRecorder.set(true)));
        info.addField(new UserInfoField(name, name, "", false, item -> clickRecorder.set(true)));
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
