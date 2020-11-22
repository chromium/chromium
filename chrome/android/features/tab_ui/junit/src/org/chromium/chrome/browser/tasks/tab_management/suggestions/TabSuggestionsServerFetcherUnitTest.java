// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests for {@link TabSuggestionsServerFetcher}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
public class TabSuggestionsServerFetcherUnitTest {
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final TabContext.TabInfo TAB_INFO_YANDEX = new TabContext.TabInfo(
            0, "Yandex", "https://www.yandex.com", "", "", 1588817215266L, "");
    private static final TabContext.TabInfo TAB_INFO_BING =
            new TabContext.TabInfo(1, "Bing", "https://www.bing.com", "", "", 1588817244592L, "");
    private static final TabContext.TabInfo TAB_INFO_AMAZON = new TabContext.TabInfo(
            2, "Amazon.com", "https://www.amazon.com", "", "", 1588817215266L, "");
    private static final TabContext.TabInfo TAB_INFO_GOOGLE = new TabContext.TabInfo(
            3, "Google", "https://www.google.com", "", "", 1588817256727L, "");
    private static final TabContext.TabInfo TAB_INFO_INCOGNITO = new TabContext.TabInfo(
            4, "Incognito site", "https://www.incognito.com", "", "", 158881926727L, "");
    private static final TabContext TAB_CONTEXT_GROUP_ALL =
            new TabContext(Arrays.asList(TAB_INFO_YANDEX, TAB_INFO_BING, TAB_INFO_GOOGLE),
                    Collections.emptyList());
    private static final TabContext TAB_CONTEXT_GROUP_SOME = new TabContext(
            Arrays.asList(TAB_INFO_YANDEX, TAB_INFO_BING, TAB_INFO_AMAZON, TAB_INFO_GOOGLE),
            Collections.emptyList());
    private static final TabContext TAB_CONTEXT_GROUP_NONE =
            new TabContext(Arrays.asList(TAB_INFO_AMAZON), Collections.emptyList());
    private static final TabContext TAB_CONTEXT_GROUP_EMPTY =
            new TabContext(Collections.emptyList(), Collections.emptyList());
    private static final TabContext.TabInfo[] EXPECTED_GROUP =
            new TabContext.TabInfo[] {TAB_INFO_YANDEX, TAB_INFO_BING, TAB_INFO_GOOGLE};
    private static final String ENDPOINT_GROUPS_RESPONSE = "{\"suggestions\":[{\"tabs\":[{\"id\":"
            + "0,\"referrer\":\"\",\"timestamp\":\"1588817215266\",\"title\":\"Yandex\",\"url\":"
            + "\"https://yandex.com/\"},{\"id\":1,\"referrer\":\"\",\"timestamp\":\"1588817244592\","
            + "\"title\":\"Bing\",\"url\":\"https://www.bing.com/\"},{\"id\":3,\"referrer\":\"\","
            + "\"timestamp\":\"1588817256727\",\"title\":\"Google\",\"url\":\"https://www.google.com/\"}]"
            + ",\"providerName\":\"VascoJourneyInference\",\"action\":\"Group\"}]}";
    private static final String EMPTY_RESPONSE = "{}";
    private static final String UNPARSEABLE_RESPONSE = "asdf";

    private static final String EXPECTED_ENDPOINT_URL =
            "https://memex-pa.googleapis.com/v1/suggestions";
    private static final String EXPECTED_CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EXPECTED_METHOD = "POST";
    private static final long EXPECTED_TIMEOUT = 10000L;

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfile;

    private TabSuggestionsServerFetcher mTabSuggestionsServerFetcher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        doReturn(false).when(mProfile).isOffTheRecord();
        mTabSuggestionsServerFetcher = new TabSuggestionsServerFetcher(mProfile);
    }

    private void mockEndpointResponse(String response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[7];
                callback.onResult(new EndpointResponse(response));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), any(Callback.class));
    }

    private void verifyEndpointArguments() {
        verify(mEndpointFetcherJniMock)
                .nativeFetchChromeAPIKey(eq(mProfile), eq(EXPECTED_ENDPOINT_URL),
                        eq(EXPECTED_METHOD), eq(EXPECTED_CONTENT_TYPE), any(String.class),
                        eq(EXPECTED_TIMEOUT), any(String[].class), any(Callback.class));
    }

    @Test
    public void testGroupAll() {
        mockEndpointResponse(ENDPOINT_GROUPS_RESPONSE);
        mTabSuggestionsServerFetcher.fetch(TAB_CONTEXT_GROUP_ALL, (res) -> {
            Assert.assertEquals(1, res.tabSuggestions.size());
            Assert.assertEquals(3, res.tabSuggestions.get(0).getTabsInfo().size());
            List<Integer> foundIds = getIds(res.tabSuggestions.get(0).getTabsInfo());
            for (int expectedId :
                    new int[] {TAB_INFO_YANDEX.id, TAB_INFO_BING.id, TAB_INFO_GOOGLE.id}) {
                Assert.assertTrue(foundIds.contains(expectedId));
            }
        });
        verifyEndpointArguments();
    }

    private static List<Integer> getIds(List<TabContext.TabInfo> tabs) {
        List<Integer> ids = new ArrayList<>();
        for (TabContext.TabInfo tab : tabs) {
            ids.add(tab.id);
        }
        return ids;
    }

    @Test
    public void testGroupSome() {
        mockEndpointResponse(ENDPOINT_GROUPS_RESPONSE);
        mTabSuggestionsServerFetcher.fetch(TAB_CONTEXT_GROUP_SOME, (res) -> {
            // TAB_INFO_AMAZON should not be included in the group because it is not a search engine
            Assert.assertEquals(1, res.tabSuggestions.size());
            Assert.assertEquals(3, res.tabSuggestions.get(0).getTabsInfo().size());
            List<Integer> foundIds = getIds(res.tabSuggestions.get(0).getTabsInfo());
            for (int expectedId :
                    new int[] {TAB_INFO_YANDEX.id, TAB_INFO_BING.id, TAB_INFO_GOOGLE.id}) {
                Assert.assertTrue(foundIds.contains(expectedId));
            }
        });
        verifyEndpointArguments();
    }

    @Test
    public void testGroupNullEmpty() {
        mockEndpointResponse(EMPTY_RESPONSE);
        mTabSuggestionsServerFetcher.fetch(TAB_CONTEXT_GROUP_NONE,
                (res) -> { Assert.assertEquals(0, res.tabSuggestions.size()); });
        verifyEndpointArguments();
    }

    @Test
    public void testGroupEmpty() {
        mockEndpointResponse(EMPTY_RESPONSE);
        mTabSuggestionsServerFetcher.fetch(TAB_CONTEXT_GROUP_EMPTY,
                (res) -> { Assert.assertEquals(0, res.tabSuggestions.size()); });
        verifyEndpointArguments();
    }

    @Test
    public void testJsonException() throws JSONException {
        TabContext tabContext = mock(TabContext.class);
        doThrow(new JSONException("there was a json exception"))
                .when(tabContext)
                .getUngroupedTabsJson();
        mTabSuggestionsServerFetcher.fetch(tabContext, (res) -> {
            Assert.assertNotNull(res.tabSuggestions);
            Assert.assertEquals(0, res.tabSuggestions.size());
        });
    }

    @Test
    public void testUnparseableResponse() {
        mockEndpointResponse(UNPARSEABLE_RESPONSE);
        mTabSuggestionsServerFetcher.fetch(TAB_CONTEXT_GROUP_ALL,
                (res) -> { Assert.assertEquals(0, res.tabSuggestions.size()); });
        verifyEndpointArguments();
    }

    @Test
    public void testServerFetcherEnabled() {
        for (boolean isSignedIn : new boolean[] {false, true}) {
            for (boolean isServerFetcherFlagEnabled : new boolean[] {false, true}) {
                TabSuggestionsServerFetcher fetcher = spy(new TabSuggestionsServerFetcher());
                doReturn(isSignedIn).when(fetcher).isSignedIn();
                doReturn(isServerFetcherFlagEnabled).when(fetcher).isServerFetcherFlagEnabled();
                if (isSignedIn && isServerFetcherFlagEnabled) {
                    Assert.assertTrue(fetcher.isEnabled());
                } else {
                    Assert.assertFalse(fetcher.isEnabled());
                }
            }
        }
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void testServerFetcherDisabledWithDisableGroup() {
        TabSuggestionsServerFetcher fetcher = spy(new TabSuggestionsServerFetcher());
        doReturn(true).when(fetcher).isSignedIn();
        doReturn(true).when(fetcher).isServerFetcherFlagEnabled();
        Assert.assertThat("The Fetcher is enabled", fetcher.isEnabled(), is(false));
    }

    @Test
    public void testServerFetcherDisabledIncognito() throws InterruptedException {
        TabContext tabContext =
                new TabContext(Arrays.asList(TAB_INFO_INCOGNITO), Collections.emptyList());
        mTabSuggestionsServerFetcher.fetch(tabContext, (fetcherResults) -> {
            Assert.assertTrue(fetcherResults.tabSuggestions.isEmpty());
        });
    }
}
