// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewStub;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the RevampedContextMenu
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class RevampedContextMenuRenderTest extends DummyUiActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private ModelListAdapter mAdapter;
    private ModelList mListItems;
    private View mView;
    private View mFrame;

    public RevampedContextMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mListItems = new ModelList();
        mAdapter = new ModelListAdapter(mListItems);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(R.layout.context_menu_fullscreen_container);
            mView = getActivity().findViewById(android.R.id.content);
            ((ViewStub) mView.findViewById(R.id.context_menu_stub)).inflate();
            mFrame = mView.findViewById(R.id.context_menu_frame);
            RevampedContextMenuListView listView = mView.findViewById(R.id.context_menu_list_view);
            listView.setAdapter(mAdapter);

            // clang-format off
            mAdapter.registerType(
                    ListItemType.HEADER,
                    new LayoutViewBuilder(R.layout.revamped_context_menu_header),
                    RevampedContextMenuHeaderViewBinder::bind);
            mAdapter.registerType(
                    ListItemType.DIVIDER,
                    new LayoutViewBuilder(R.layout.app_menu_divider),
                    (m, v, p) -> {
                    });
            mAdapter.registerType(
                    ListItemType.CONTEXT_MENU_ITEM,
                    new LayoutViewBuilder(R.layout.revamped_context_menu_row),
                    RevampedContextMenuItemViewBinder::bind);
            mAdapter.registerType(
                    ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                    new LayoutViewBuilder(R.layout.revamped_context_menu_share_row),
                    RevampedContextMenuItemWithIconButtonViewBinder::bind);
            // clang-format on
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NightModeTestUtils.tearDownNightModeForDummyUiActivity();
            mListItems.clear();
        });
        super.tearDownTest();
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRevampedContextMenuViewWithLink() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mListItems.add(
                    new ListItem(ListItemType.HEADER, getHeaderModel("", "www.google.com", false)));
            mListItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            mListItems.add((
                    new ListItem(ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in new tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in incognito tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Copy link address"))));
            mListItems.add((new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                    getShareItemModel("Share link"))));
        });
        mRenderTestRule.render(mFrame, "revamped_context_menu_with_link");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRevampedContextMenuViewWithImageLink() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mListItems.add(new ListItem(
                    ListItemType.HEADER, getHeaderModel("Capybara", "www.google.com", true)));
            mListItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            mListItems.add((
                    new ListItem(ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in new tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in incognito tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Copy link address"))));
            mListItems.add((new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                    getShareItemModel("Share link"))));
            mListItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open image in new tab"))));
            mListItems.add(
                    (new ListItem(ListItemType.CONTEXT_MENU_ITEM, getItemModel("Download image"))));
            mListItems.add((new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                    getShareItemModel("Share image"))));

        });
        mRenderTestRule.render(mFrame, "revamped_context_menu_with_image_link");
    }

    private PropertyModel getHeaderModel(String title, CharSequence url, boolean hasImage) {
        Bitmap image;
        if (hasImage) {
            image = BitmapFactory.decodeFile(
                    UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/capybara.jpg"));
        } else {
            final int size = getActivity().getResources().getDimensionPixelSize(
                    R.dimen.revamped_context_menu_header_monogram_size);
            image = BitmapFactory.decodeFile(UrlUtils.getIsolatedTestFilePath(
                    "chrome/test/data/android/UiCapture/cloud.png"));
            image = Bitmap.createScaledBitmap(image, size, size, true);
        }

        return new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                .with(RevampedContextMenuHeaderProperties.TITLE, title)
                .with(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES, 1)
                .with(RevampedContextMenuHeaderProperties.URL, url)
                .with(RevampedContextMenuHeaderProperties.URL_MAX_LINES, 1)
                .with(RevampedContextMenuHeaderProperties.IMAGE, image)
                .with(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, !hasImage)
                .build();
    }

    private PropertyModel getItemModel(String title) {
        return new PropertyModel.Builder(RevampedContextMenuItemProperties.ALL_KEYS)
                .with(RevampedContextMenuItemProperties.TEXT, title)
                .build();
    }

    private PropertyModel getShareItemModel(String title) {
        final BitmapDrawable drawable = new BitmapDrawable(getActivity().getResources(),
                BitmapFactory.decodeFile(UrlUtils.getIsolatedTestFilePath(
                        "chrome/test/data/android/UiCapture/dots.png")));
        return new PropertyModel.Builder(RevampedContextMenuItemWithIconButtonProperties.ALL_KEYS)
                .with(RevampedContextMenuItemWithIconButtonProperties.TEXT, title)
                .with(RevampedContextMenuItemWithIconButtonProperties.BUTTON_IMAGE, drawable)
                .build();
    }
}
