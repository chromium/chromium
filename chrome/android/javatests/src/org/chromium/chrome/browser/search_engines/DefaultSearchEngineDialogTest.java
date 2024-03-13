// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.search_engines.FakeTemplateUrl;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;
import java.util.List;

/** Integration tests for the {@link DefaultSearchEnginePromoDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class DefaultSearchEngineDialogTest {
    @Rule
    public BaseActivityTestRule mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Callback<Boolean> mOnSuccessCallback;

    private final FakeTemplateUrl mEngine1 = new FakeTemplateUrl("EngineOne", "EngineOneKeyword");
    private final FakeTemplateUrl mEngine2 = new FakeTemplateUrl("EngineTwo", "EngineTwoKeyword");

    @Before
    public void setUp() throws Exception {
        final CallbackHelper templateUrlServiceInit = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

                    LocaleManagerDelegate fakeDelegate =
                            new LocaleManagerDelegate() {
                                @Override
                                public List<TemplateUrl> getSearchEnginesForPromoDialog(
                                        int promoType) {
                                    return getTemplateUrlService().getTemplateUrls();
                                }
                            };
                    LocaleManager.getInstance().setDelegateForTest(fakeDelegate);

                    getTemplateUrlService()
                            .registerLoadListener(
                                    new TemplateUrlService.LoadListener() {
                                        @Override
                                        public void onTemplateUrlServiceLoaded() {
                                            getTemplateUrlService().unregisterLoadListener(this);
                                            templateUrlServiceInit.notifyCalled();
                                        }
                                    });
                });
        templateUrlServiceInit.waitForFirst();
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @LargeTest
    public void testDialogView() {
        showDialog();

        onView(withText(R.string.search_engine_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(mEngine1.getShortName())).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(mEngine2.getShortName())).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.primary_button)).inRoot(isDialog()).check(matches(not(isEnabled())));
    }

    @Test
    @LargeTest
    public void testButtonClickRunsCallback() {
        showDialog();
        onView(withText(R.string.search_engine_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        onView(withText(mEngine1.getShortName())).inRoot(isDialog()).perform(click());
        onView(withId(R.id.primary_button)).inRoot(isDialog()).perform(click());

        verify(mOnSuccessCallback).onResult(true);
    }

    @Test
    @LargeTest
    public void testButtonClickDismissesDialog() {
        showDialog();
        onView(withText(R.string.search_engine_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        onView(withText(mEngine1.getShortName())).inRoot(isDialog()).perform(click());
        onView(withId(R.id.primary_button)).inRoot(isDialog()).perform(click());

        onView(withText(R.string.search_engine_dialog_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    public void testBackPressDoesNotDismissDialog() {
        showDialog();
        onView(withText(R.string.search_engine_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        Espresso.pressBack();

        onView(withText(R.string.search_engine_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    private void showDialog() {
        DefaultSearchEngineDialogHelper.Delegate delegate =
                new DefaultSearchEngineDialogHelper.Delegate() {
                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(
                            @SearchEnginePromoType int type) {
                        return Arrays.asList(mEngine1, mEngine2);
                    }

                    @Override
                    public void onUserSearchEngineChoice(
                            @SearchEnginePromoType int type,
                            List<String> keywords,
                            String keyword) {}
                };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new DefaultSearchEngineDialogCoordinator(
                                    mActivityTestRule.getActivity(),
                                    delegate,
                                    SearchEnginePromoType.SHOW_EXISTING,
                                    mOnSuccessCallback)
                            .show();
                });
    }

    private TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile());
    }
}
