// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.LargeTest;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;

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
import org.chromium.chrome.browser.night_mode.NightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.ArrayList;
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
    public RenderTestRule mRenderTestRule = new RenderTestRule();

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
            getActivity().setContentView(R.layout.revamped_context_menu);
            mView = getActivity().findViewById(android.R.id.content);
            mFrame = mView.findViewById(R.id.context_menu_frame);
            RevampedContextMenuListView listView = mView.findViewById(R.id.context_menu_list_view);
            listView.setAdapter(mAdapter);

            // clang-format off
            mAdapter.registerType(
                    ListItemType.HEADER,
                    () -> LayoutInflater.from(listView.getContext())
                            .inflate(R.layout.revamped_context_menu_header, null),
                    RevampedContextMenuHeaderViewBinder::bind);
            mAdapter.registerType(
                    ListItemType.DIVIDER,
                    () -> LayoutInflater.from(listView.getContext())
                            .inflate(R.layout.context_menu_divider, null),
                    (m, v, p) -> {
                    });
            mAdapter.registerType(
                    ListItemType.CONTEXT_MENU_ITEM,
                    () -> LayoutInflater.from(listView.getContext())
                            .inflate(R.layout.revamped_context_menu_row, null),
                    RevampedContextMenuItemViewBinder::bind);
            mAdapter.registerType(
                    ListItemType.CONTEXT_MENU_SHARE_ITEM,
                    () -> LayoutInflater.from(listView.getContext())
                            .inflate(R.layout.revamped_context_menu_share_row, null),
                    RevampedContextMenuShareItemViewBinder::bind);
            // clang-format on
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForDummyUiActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> mListItems.clear());
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
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_SHARE_ITEM, getShareItemModel("Share link"))));
        });
        mRenderTestRule.render(mFrame, "revamped_context_menu_with_link");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRevampedContextMenuViewWithImageLink() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<Pair<Integer, PropertyModel>> itemList = new ArrayList<>();
            mListItems.add(new ListItem(
                    ListItemType.HEADER, getHeaderModel("Capybara", "www.google.com", true)));
            mListItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            mListItems.add((
                    new ListItem(ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in new tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open in incognito tab"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Copy link address"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_SHARE_ITEM, getShareItemModel("Share link"))));
            mListItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_ITEM, getItemModel("Open image in new tab"))));
            mListItems.add(
                    (new ListItem(ListItemType.CONTEXT_MENU_ITEM, getItemModel("Download image"))));
            mListItems.add((new ListItem(
                    ListItemType.CONTEXT_MENU_SHARE_ITEM, getShareItemModel("Share image"))));

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
        return new PropertyModel.Builder(RevampedContextMenuShareItemProperties.ALL_KEYS)
                .with(RevampedContextMenuShareItemProperties.TEXT, title)
                .with(RevampedContextMenuShareItemProperties.IMAGE, drawable)
                .build();
    }
}
