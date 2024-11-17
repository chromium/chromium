// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.core.util.Pair;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.time.Clock;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/** Render tests for {@link TabGroupRowView}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabGroupRowViewRenderTest {

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(2)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FaviconResolver mFaviconResolver;

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);

        Map<GURL, Integer> urlToColor =
                Map.of(
                        JUnitTestGURLs.RED_1,
                        Color.RED,
                        JUnitTestGURLs.URL_1,
                        Color.GREEN,
                        JUnitTestGURLs.BLUE_1,
                        Color.BLUE,
                        JUnitTestGURLs.URL_2,
                        Color.BLACK);

        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    GURL url = (GURL) invocation.getArguments()[0];
                                    Callback<Drawable> callback =
                                            (Callback<Drawable>) invocation.getArguments()[1];
                                    callback.onResult(new ColorDrawable(urlToColor.get(url)));
                                    return null;
                                })
                .when(mFaviconResolver)
                .resolve(any(), any());
    }

    private void setUpOnUi() {
        mTabGroupRowView = inflateAndAttach(mActivity, R.layout.tab_group_row);
    }

    private <T extends View> T inflateAndAttach(Context context, @LayoutRes int layoutRes) {
        FrameLayout contentView = new FrameLayout(mActivity);
        contentView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mActivity.setContentView(contentView);

        LayoutInflater inflater = LayoutInflater.from(context);
        inflater.inflate(layoutRes, contentView);
        assert contentView.getChildCount() == 1;
        return (T) contentView.getChildAt(0);
    }

    private ClusterData makeCornerData(GURL... urls) {
        List<GURL> firstUrls =
                Arrays.stream(urls)
                        .limit(TabGroupFaviconCluster.CORNER_COUNT)
                        .collect(Collectors.toList());
        return new ClusterData(mFaviconResolver, urls.length, firstUrls);
    }

    private void remakeWithUrls(GURL... urls) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(CLUSTER_DATA, makeCornerData(urls));
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(TITLE_DATA, new Pair<>("Title", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, TabGroupRowViewBinder::bind);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithVeryLongTitle() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
                    builder.with(CLUSTER_DATA, makeCornerData(JUnitTestGURLs.RED_1));
                    builder.with(TabGroupRowProperties.COLOR_INDEX, TabGroupColorId.GREY);
                    builder.with(
                            TITLE_DATA,
                            new Pair<>(
                                    "VeryLongTitleThatGetsTruncatedOrSplitOverMultipleLines", 1));
                    builder.with(CREATION_MILLIS, Clock.systemUTC().millis());
                    mPropertyModel = builder.build();
                    PropertyModelChangeProcessor.create(
                            mPropertyModel, mTabGroupRowView, TabGroupRowViewBinder::bind);
                });
        mRenderTestRule.render(mTabGroupRowView, "long_title");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithVariousFaviconCounts() throws Exception {
        remakeWithUrls(
                JUnitTestGURLs.RED_1,
                JUnitTestGURLs.URL_1,
                JUnitTestGURLs.BLUE_1,
                JUnitTestGURLs.URL_2,
                JUnitTestGURLs.URL_3);
        mRenderTestRule.render(mTabGroupRowView, "five");

        remakeWithUrls(
                JUnitTestGURLs.RED_1,
                JUnitTestGURLs.URL_1,
                JUnitTestGURLs.BLUE_1,
                JUnitTestGURLs.URL_2);
        mRenderTestRule.render(mTabGroupRowView, "four");

        remakeWithUrls(JUnitTestGURLs.RED_1, JUnitTestGURLs.URL_1, JUnitTestGURLs.BLUE_1);
        mRenderTestRule.render(mTabGroupRowView, "three");

        remakeWithUrls(JUnitTestGURLs.RED_1, JUnitTestGURLs.URL_1);
        mRenderTestRule.render(mTabGroupRowView, "two");

        remakeWithUrls(JUnitTestGURLs.RED_1);
        mRenderTestRule.render(mTabGroupRowView, "one");
    }
}
