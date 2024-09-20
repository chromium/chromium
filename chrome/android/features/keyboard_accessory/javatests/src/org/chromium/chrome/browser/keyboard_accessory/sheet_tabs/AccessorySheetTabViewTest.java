// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;

import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/** View tests for the password accessory sheet. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetTabViewTest {
    private AccessorySheetTabItemsModel mModel;
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * This helper method inflates the accessory sheet and loads the given layout as minimalistic
     * Tab. The passed callback then allows access to the inflated layout.
     *
     * @param layout The layout to be inflated.
     * @param listener Is called with the inflated layout when the Accessory Sheet initializes it.
     */
    private void openLayoutInAccessorySheet(
            @LayoutRes int layout, KeyboardAccessoryData.Tab.Listener listener) {
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
                                        layout,
                                        AccessoryTabType.ALL,
                                        listener)
                            });
                    accessorySheet.setHeight(
                            mActivityTestRule
                                    .getActivity()
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.keyboard_accessory_sheet_height));
                    accessorySheet.show();
                });
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        openLayoutInAccessorySheet(
                R.layout.password_accessory_sheet,
                new KeyboardAccessoryData.Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        mView.set((RecyclerView) view);
                        AccessorySheetTabViewBinder.initializeView(mView.get(), null);
                        ((RecyclerView) view)
                                .setAdapter(
                                        new RecyclerViewAdapter<>(
                                                new SimpleRecyclerViewMcp<>(
                                                        mModel,
                                                        AccessorySheetDataPiece::getType,
                                                        AccessorySheetTabViewBinder
                                                                        .ElementViewHolder
                                                                ::bind),
                                                AccessorySheetTabViewBinder::create));
                    }

                    @Override
                    public void onTabShown() {}
                });
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get(), notNullValue()));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingFooterCommandToTheModelRendersButton() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.add(
                            new AccessorySheetDataPiece(
                                    new KeyboardAccessoryData.FooterCommand(
                                            "Manage passwords", item -> clicked.set(true)),
                                    Type.FOOTER_COMMAND));
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mView.get().getChildCount(), is(1)));
        assertThat(mView.get().getChildAt(0), instanceOf(TextView.class));
        TextView btn = (TextView) mView.get().getChildAt(0);

        assertThat(btn.getText(), is("Manage passwords"));

        ThreadUtils.runOnUiThreadBlocking(btn::performClick);
        assertThat(clicked.get(), is(true));
    }
}
