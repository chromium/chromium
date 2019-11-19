// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.LayoutRes;

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
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;
import org.chromium.ui.widget.TextViewWithLeading;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the password accessory sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetTabViewTest {
    private final AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * This helper method inflates the accessory sheet and loads the given layout as minimalistic
     * Tab. The passed callback then allows access to the inflated layout.
     * @param layout The layout to be inflated.
     * @param listener Is called with the inflated layout when the Accessory Sheet initializes it.
     */
    private void openLayoutInAccessorySheet(
            @LayoutRes int layout, KeyboardAccessoryData.Tab.Listener listener) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccessorySheetCoordinator accessorySheet =
                    new AccessorySheetCoordinator(mActivityTestRule.getActivity().findViewById(
                            R.id.keyboard_accessory_sheet_stub));
            accessorySheet.setTabs(new KeyboardAccessoryData.Tab[] {new KeyboardAccessoryData.Tab(
                    "Passwords", null, null, layout, AccessoryTabType.ALL, listener)});
            accessorySheet.setHeight(
                    mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                            R.dimen.keyboard_accessory_sheet_height));
            accessorySheet.show();
        });
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        openLayoutInAccessorySheet(
                R.layout.password_accessory_sheet, new KeyboardAccessoryData.Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        mView.set((RecyclerView) view);
                        AccessorySheetTabViewBinder.initializeView(mView.get(), null);
                        ((RecyclerView) view)
                                .setAdapter(new RecyclerViewAdapter<>(
                                        new SimpleRecyclerViewMcp<>(mModel,
                                                AccessorySheetDataPiece::getType,
                                                AccessorySheetTabViewBinder
                                                        .ElementViewHolder::bind),
                                        AccessorySheetTabViewBinder::create));
                    }

                    @Override
                    public void onTabShown() {}
                });
        CriteriaHelper.pollUiThread(Criteria.equals(true, () -> mView.get() != null));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingATitleToTheModelRendersIt() {
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.add(new AccessorySheetDataPiece("Passwords", Type.TITLE)); });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        assertThat(mView.get().getChildAt(0), instanceOf(LinearLayout.class));
        LinearLayout layout = (LinearLayout) mView.get().getChildAt(0);
        assertThat(layout.getChildCount(), is(3));
        assertThat(layout.getChildAt(0), instanceOf(View.class)); // The top divider.
        assertThat(layout.getChildAt(1), instanceOf(TextViewWithLeading.class));
        assertThat(layout.getChildAt(2), instanceOf(View.class)); // Divider to commands.
        assertThat(((TextView) layout.getChildAt(1)).getText(), is("Passwords"));
    }

    @Test
    @MediumTest
    public void testAddingFooterCommandToTheModelRendersButton() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(
                    new KeyboardAccessoryData.FooterCommand(
                            "Manage passwords", item -> clicked.set(true)),
                    Type.FOOTER_COMMAND));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        assertThat(mView.get().getChildAt(0), instanceOf(TextView.class));
        TextView btn = (TextView) mView.get().getChildAt(0);

        assertThat(btn.getText(), is("Manage passwords"));

        TestThreadUtils.runOnUiThreadBlocking(btn::performClick);
        assertThat(clicked.get(), is(true));
    }
}
