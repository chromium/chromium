// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.mockito.Mockito.doReturn;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.app.Activity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.List;

/** Tests for Search Engine Settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SearchEngineSettingsRenderTest {

    public final @Rule BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public final @Rule TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    public final @Rule ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    public final @Rule MockitoRule mMocks = MockitoJUnit.rule();

    private @Mock TemplateUrlService mMockTemplateUrlService;
    private @Mock Profile mProfile;

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testRenderWithSecFeature() throws Exception {
        testRender("search_engine_settings");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testRenderWithoutSecFeature() throws Exception {
        testRender("search_engine_settings_flag_off");
    }

    private void testRender(String screenshotId) throws Exception {
        TemplateUrl defaultSearchEngine = buildTemplateUrl("Custom Engine", 0);
        List<TemplateUrl> templateUrls =
                List.of(defaultSearchEngine, buildTemplateUrl("Prepopulated Engine", 2));

        doReturn(new ArrayList<>(templateUrls)).when(mMockTemplateUrlService).getTemplateUrls();
        doReturn(defaultSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        doReturn(true).when(mMockTemplateUrlService).isEeaChoiceCountry();
        doReturn(true).when(mMockTemplateUrlService).isLoaded();

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);

        mActivityTestRule.launchActivity(null);

        View view =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Activity activity = mActivityTestRule.getActivity();
                            SearchEngineAdapter adapter =
                                    new SearchEngineAdapter(activity, mProfile);
                            adapter.start();

                            LinearLayout container = new LinearLayout(activity);
                            container.setLayoutParams(
                                    new LinearLayout.LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
                            container.setOrientation(LinearLayout.VERTICAL);

                            container.addView(adapter.getView(0, null, container));
                            container.addView(adapter.getView(1, null, container));

                            activity.setContentView(container);
                            return container;
                        });

        mRenderTestRule.render(view, screenshotId);
    }

    private static TemplateUrl buildTemplateUrl(String shortName, int prepopulatedId) {
        TemplateUrl templateUrl =
                buildMockTemplateUrl("prepopulatedId=" + prepopulatedId, prepopulatedId);
        doReturn(shortName).when(templateUrl).getShortName();
        return templateUrl;
    }
}
