// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.TabResumptionDataProviderFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleCoordinatorUnitTest extends TestSupportExtended {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UrlImageProvider mUrlImageProvide;
    @Mock private TabResumptionDataProvider mProvider;
    @Mock private TabModelSelector mTabModelSelector;

    private TabResumptionDataProviderFactory mDataProviderFactory;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;

    private TabResumptionModuleCoordinator mCoordinator;

    SuggestionsResult mResults;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        mTabModelSelectorSupplier = new ObservableSupplierImpl<>();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(() -> CURRENT_TIME_MS);

        mProvider =
                new TabResumptionDataProvider() {
                    @Override
                    public void destroy() {}

                    @Override
                    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
                        suggestionsCallback.onResult(mResults);
                    }
                };

        mDataProviderFactory = () -> mProvider;

        mCoordinator =
                new TabResumptionModuleCoordinator(
                        /* context= */ mContext,
                        /* moduleDelegate= */ mModuleDelegate,
                        /* tabModelSelectorSupplier= */ mTabModelSelectorSupplier,
                        /* dataProviderFactory= */ mDataProviderFactory,
                        /* urlImageProvider= */ mUrlImageProvide);
        mModel = mCoordinator.getModelForTesting();

        Assert.assertEquals(ModuleType.TAB_RESUMPTION, mCoordinator.getModuleType());

        Assert.assertNull(mModel.get(TabResumptionModuleProperties.TITLE));
        Assert.assertNull(mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE));

        checkModuleState(
                /* isVisible= */ false,
                /* expectOnDataReadyCalls= */ 0,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);
    }

    @After
    public void tearDown() {
        mCoordinator.hideModule();

        mCoordinator.destroy();
        TabResumptionModuleUtils.setFakeCurrentTimeMsForTesting(null);
    }

    @Test
    @SmallTest
    public void testShowTwoSuggestionsUpdateToOne() {
        // Two suggestions.
        mResults =
                new SuggestionsResult(
                        ResultStrength.STABLE,
                        Arrays.asList(makeSyncDerivedSuggestion(0), makeSyncDerivedSuggestion(1)));
        mCoordinator.showModule();
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);

        Assert.assertEquals(
                "Continue with these tabs", mModel.get(TabResumptionModuleProperties.TITLE));
        Assert.assertEquals(
                "Hide continue with these tabs card",
                mCoordinator.getModuleContextMenuHideText(mContext));

        SuggestionBundle bundle2 = mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        Assert.assertEquals("Google Dog", bundle2.entries.get(0).title);
        Assert.assertEquals("Google Cat", bundle2.entries.get(1).title);

        // Update: One suggestion.
        mResults =
                new SuggestionsResult(
                        ResultStrength.STABLE, Arrays.asList(makeSyncDerivedSuggestion(1)));
        mCoordinator.updateModule();
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);

        Assert.assertEquals(
                "Continue with this tab", mModel.get(TabResumptionModuleProperties.TITLE));
        Assert.assertEquals(
                "Hide continue with this tab card",
                mCoordinator.getModuleContextMenuHideText(mContext));

        SuggestionBundle bundle1 = mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        Assert.assertNotNull(bundle1);
        Assert.assertEquals("Google Cat", bundle1.entries.get(0).title);
    }

    @Test
    @SmallTest
    public void testClickSyncDerivedTab() {
        mResults =
                new SuggestionsResult(
                        ResultStrength.STABLE, Arrays.asList(makeSyncDerivedSuggestion(1)));
        mCoordinator.showModule();
        SuggestionEntry entry = clickOnEntry(0);
        verify(mModuleDelegate, times(1)).onUrlClicked(entry.url, ModuleType.TAB_RESUMPTION);
    }

    @Test
    @SmallTest
    public void testClickLocalTab() {
        Tab tab = createMockLocalTab(0);
        mResults =
                new SuggestionsResult(
                        ResultStrength.STABLE,
                        Arrays.asList(SuggestionEntry.createFromLocalTab(tab)));
        mCoordinator.showModule();
        checkModuleState(
                /* isVisible= */ true,
                /* expectOnDataReadyCalls= */ 1,
                /* expectOnDataFetchFailedCalls= */ 0,
                /* expectRemoveModuleCalls= */ 0);

        SuggestionEntry entry = clickOnEntry(0);
        verify(mModuleDelegate, times(1))
                .onTabClicked(entry.getLocalTabId(), ModuleType.TAB_RESUMPTION);
    }

    @Test
    @SmallTest
    public void testClickSeeMore() {
        mResults =
                new SuggestionsResult(
                        ResultStrength.STABLE, Arrays.asList(makeSyncDerivedSuggestion(1)));
        mCoordinator.showModule();

        Runnable seeMoreLinkClickCallback =
                mModel.get(TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK);
        seeMoreLinkClickCallback.run();

        verify(mModuleDelegate, times(1))
                .onUrlClicked(new GURL(UrlConstants.RECENT_TABS_URL), ModuleType.TAB_RESUMPTION);
    }

    /**
     * Clicks a suggestion in the SuggestionBundle stored in `mModel`.
     *
     * @param index The index into in the SuggestionBundle stored in `mModel`
     * @return The entry specified by `index`.
     */
    private SuggestionEntry clickOnEntry(int index) {
        SuggestionBundle bundle = mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        SuggestionEntry entry = bundle.entries.get(index);
        SuggestionClickCallback clickCallback =
                mModel.get(TabResumptionModuleProperties.CLICK_CALLBACK);
        clickCallback.onSuggestionClicked(entry);
        return entry;
    }
}
