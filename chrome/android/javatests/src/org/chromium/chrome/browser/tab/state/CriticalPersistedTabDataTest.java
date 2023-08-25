// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import com.google.flatbuffers.FlatBufferBuilder;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.flatbuffer.CriticalPersistedTabDataFlatBuffer;
import org.chromium.chrome.browser.tab.flatbuffer.CriticalPersistedTabDataFlatBufferTest;
import org.chromium.chrome.browser.tab.flatbuffer.LaunchTypeAtCreation;
import org.chromium.chrome.browser.tab.flatbuffer.LaunchTypeAtCreationTest;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentTypeTest;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;

/**
 * Test relating to {@link CriticalPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CriticalPersistedTabDataTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final int TAB_ID = 1;
    private static final int PARENT_ID = 2;
    private static final int ROOT_ID = 3;
    private static final int CONTENT_STATE_VERSION = 42;
    private static final byte[] WEB_CONTENTS_STATE_BYTES = {9, 10};
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(ByteBuffer.allocateDirect(WEB_CONTENTS_STATE_BYTES.length));
    private static final long TIMESTAMP = 203847028374L;
    private static final long LAST_NAVIGATION_COMMITTED_TIMESTAMP = 3141592653589L;
    private static final String APP_ID = "AppId";
    private static final String OPENER_APP_ID = "OpenerAppId";
    private static final int THEME_COLOR = 5;
    private static final Integer LAUNCH_TYPE_AT_CREATION = 3;
    private static final @TabUserAgent int USER_AGENT_A = TabUserAgent.MOBILE;
    private static final @TabUserAgent int USER_AGENT_B = TabUserAgent.DESKTOP;
    private static final String TITLE_A = "original title";
    private static final String TITLE_B = "new title";
    private static final GURL URL_A = new GURL("https://a.com");
    private static final GURL URL_B = new GURL("https://b.com");
    private static final int ROOT_ID_A = 42;
    private static final int ROOT_ID_B = 1;
    private static final int PARENT_ID_A = 43;
    private static final int PARENT_ID_B = 2;
    private static final long TIMESTAMP_A = 44;
    private static final long TIMESTAMP_B = 3;
    private static final @TabLaunchType Integer TAB_LAUNCH_TYPE_A = TabLaunchType.FROM_LINK;
    private static final @TabLaunchType Integer TAB_LAUNCH_TYPE_B = TabLaunchType.FROM_EXTERNAL_APP;
    private static final byte[] WEB_CONTENTS_STATE_A_BYTES = {4};
    private static final byte[] WEB_CONTENTS_STATE_B_BYTES = {5, 6};
    private static final WebContentsState WEB_CONTENTS_STATE_A =
            new WebContentsState(ByteBuffer.allocateDirect(WEB_CONTENTS_STATE_A_BYTES.length));
    private static final WebContentsState WEB_CONTENTS_STATE_B =
            new WebContentsState(ByteBuffer.allocateDirect(WEB_CONTENTS_STATE_B_BYTES.length));
    private static final String EXPECTED_TITLE = "My_title";
    private static final String MOCK_DATA_ID = "mock-id";

    static {
        WEB_CONTENTS_STATE.buffer().put(WEB_CONTENTS_STATE_BYTES);
        WEB_CONTENTS_STATE_A.buffer().put(WEB_CONTENTS_STATE_A_BYTES);
        WEB_CONTENTS_STATE_B.buffer().put(WEB_CONTENTS_STATE_B_BYTES);
    }

    private CriticalPersistedTabData mCriticalPersistedTabData;
    private MockPersistedTabDataStorage mStorage;
    private EmbeddedTestServer mTestServer;

    // Tell R8 not to break the ability to mock these classes.
    @Mock
    private TabImpl mUnused1;
    @Mock
    private CriticalPersistedTabData mUnused2;

    @Spy
    private MockPersistedTabDataStorage mMockPersistedTabDataStorageSpy;

    private static Tab mockTab(int id, boolean isEncrypted) {
        Tab tab = MockTab.createAndInitialize(id, isEncrypted);
        tab.setIsTabSaveEnabled(true);
        return tab;
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        PersistedTabDataConfiguration.setUseTestConfig(true);
        mStorage = (MockPersistedTabDataStorage) PersistedTabDataConfiguration.getTestConfig()
                           .getStorage();
        mTestServer = sActivityTestRule.getTestServer();
    }

    @SmallTest
    @Test
    public void testNonEncryptedSaveRestore() throws InterruptedException {
        testSaveRestoreDelete(false);
    }

    @SmallTest
    @Test
    public void testEncryptedSaveRestoreDelete() throws InterruptedException {
        testSaveRestoreDelete(true);
    }

    private void testSaveRestoreDelete(boolean isEncrypted) throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        Callback<CriticalPersistedTabData> callback = new Callback<CriticalPersistedTabData>() {
            @Override
            public void onResult(CriticalPersistedTabData res) {
                ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
                supplier.set(true);
                if (res != null) {
                    res.registerIsTabSaveEnabledSupplier(supplier);
                }
                mCriticalPersistedTabData = res;
                semaphore.release();
            }
        };
        final Semaphore saveSemaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(
                    new MockTab(TAB_ID, isEncrypted), "", "", PARENT_ID, ROOT_ID, TIMESTAMP,
                    WEB_CONTENTS_STATE, CONTENT_STATE_VERSION, OPENER_APP_ID, THEME_COLOR,
                    LAUNCH_TYPE_AT_CREATION, USER_AGENT_A, LAST_NAVIGATION_COMMITTED_TIMESTAMP);
            criticalPersistedTabData.setShouldSaveForTesting(true);
            mStorage.setSemaphore(saveSemaphore);
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
            supplier.set(true);
            criticalPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
            criticalPersistedTabData.save();
            acquireSemaphore(saveSemaphore);
            CriticalPersistedTabData.from(new MockTab(TAB_ID, isEncrypted), callback);
        });
        semaphore.acquire();
        Assert.assertNotNull(mCriticalPersistedTabData);
        assertEquals(mCriticalPersistedTabData.getParentId(), PARENT_ID);
        assertEquals(mCriticalPersistedTabData.getRootId(), ROOT_ID);
        assertEquals(mCriticalPersistedTabData.getTimestampMillis(), TIMESTAMP);
        assertEquals(mCriticalPersistedTabData.getContentStateVersion(), CONTENT_STATE_VERSION);
        assertEquals(mCriticalPersistedTabData.getOpenerAppId(), OPENER_APP_ID);
        assertEquals(mCriticalPersistedTabData.getThemeColor(), THEME_COLOR);
        assertEquals(
                mCriticalPersistedTabData.getTabLaunchTypeAtCreation(), LAUNCH_TYPE_AT_CREATION);
        Assert.assertArrayEquals(CriticalPersistedTabData.getContentStateByteArray(
                                         mCriticalPersistedTabData.getWebContentsState().buffer()),
                WEB_CONTENTS_STATE_BYTES);
        assertEquals(mCriticalPersistedTabData.getUserAgent(), USER_AGENT_A);
        assertEquals(mCriticalPersistedTabData.getLastNavigationCommittedTimestampMillis(),
                LAST_NAVIGATION_COMMITTED_TIMESTAMP);
        Semaphore deleteSemaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.setSemaphore(deleteSemaphore);
            mCriticalPersistedTabData.delete();
            acquireSemaphore(deleteSemaphore);
            CriticalPersistedTabData.from(new MockTab(TAB_ID, isEncrypted), callback);
        });
        semaphore.acquire();
        Assert.assertNull(mCriticalPersistedTabData);
        // TODO(crbug.com/1060232) test restored.save() after restored.delete()
        // Also cover
        // - Multiple (different) TAB_IDs being stored in the same storage.
        // - Tests for doing multiple operations on the same TAB_ID:
        //   - save() multiple times
        //  - restore() multiple times
        //  - delete() multiple times
    }

    private static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            // Throw Runtime exception to make catching InterruptedException unnecessary
            throw new RuntimeException(e);
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTabSaving() throws Throwable {
        // Thread policies need to be relaxed - starting the test activity makes them stricter
        // and the spy will fail without the thread policy being relaxed.
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            TabImpl tab = new MockTab(1, false);
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(tab));
            tab = MockTab.initializeWithCriticalPersistedTabData(tab, spyCriticalPersistedTabData);
            tab.registerTabSaving();

            tab.setIsTabSaveEnabled(true);
            verify(spyCriticalPersistedTabData, times(1)).save();
            verify(spyCriticalPersistedTabData, times(0)).delete();

            tab.setIsTabSaveEnabled(false);
            verify(spyCriticalPersistedTabData, times(1)).save();
            verify(spyCriticalPersistedTabData, times(1)).delete();

            tab.setIsTabSaveEnabled(true);
            verify(spyCriticalPersistedTabData, times(2)).save();
            verify(spyCriticalPersistedTabData, times(1)).delete();
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveNTP() throws Throwable {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            for (boolean canGoBack : new boolean[] {false, true}) {
                for (boolean canGoForward : new boolean[] {false, true}) {
                    CriticalPersistedTabData spyCriticalPersistedTabData =
                            prepareCPTDShouldTabSave(canGoBack, canGoForward);
                    spyCriticalPersistedTabData.setShouldSave();
                    spyCriticalPersistedTabData.setUrl(new GURL(UrlConstants.NTP_URL));
                    assertEquals(
                            canGoBack || canGoForward, spyCriticalPersistedTabData.shouldSave());
                }
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveContentUrl() throws Throwable {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    prepareCPTDShouldTabSave(false, false);
            spyCriticalPersistedTabData.setUrl(new GURL("content://my_content"));
            Assert.assertFalse(spyCriticalPersistedTabData.shouldSave());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveRegularUrl() throws Throwable {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    prepareCPTDShouldTabSave(false, false);
            spyCriticalPersistedTabData.setUrl(new GURL("https://www.google.com"));
            spyCriticalPersistedTabData.setShouldSave();
            assertTrue(spyCriticalPersistedTabData.shouldSave());
        }
    }

    private CriticalPersistedTabData prepareCPTDShouldTabSave(
            boolean canGoBack, boolean canGoForward) {
        TabImpl tab = new MockTab(1, false);
        TabImpl spyTab = spy(tab);
        doReturn(canGoBack).when(spyTab).canGoBack();
        doReturn(canGoForward).when(spyTab).canGoForward();
        return spy(CriticalPersistedTabData.from(spyTab));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializationBug() throws InterruptedException {
        Tab tab = mockTab(TAB_ID, false);
        CriticalPersistedTabData criticalPersistedTabData =
                new CriticalPersistedTabData(tab, "", "", PARENT_ID, ROOT_ID, TIMESTAMP,
                        WEB_CONTENTS_STATE, CONTENT_STATE_VERSION, OPENER_APP_ID, THEME_COLOR,
                        LAUNCH_TYPE_AT_CREATION, USER_AGENT_A, LAST_NAVIGATION_COMMITTED_TIMESTAMP);
        Serializer<ByteBuffer> serializer = criticalPersistedTabData.getSerializer();
        serializer.preSerialize();
        ByteBuffer serialized = serializer.get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        CriticalPersistedTabData deserialized =
                new CriticalPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        Assert.assertNotNull(deserialized);
        assertEquals(PARENT_ID, deserialized.getParentId());
        assertEquals(ROOT_ID, deserialized.getRootId());
        assertEquals(TIMESTAMP, deserialized.getTimestampMillis());
        assertEquals(CONTENT_STATE_VERSION, deserialized.getContentStateVersion());
        assertEquals(OPENER_APP_ID, deserialized.getOpenerAppId());
        assertEquals(THEME_COLOR, deserialized.getThemeColor());
        assertEquals(LAUNCH_TYPE_AT_CREATION, deserialized.getTabLaunchTypeAtCreation());
        Assert.assertArrayEquals(WEB_CONTENTS_STATE_BYTES,
                CriticalPersistedTabData.getContentStateByteArray(
                        deserialized.getWebContentsState().buffer()));
        assertEquals(USER_AGENT_A, deserialized.getUserAgent());
        assertEquals(LAST_NAVIGATION_COMMITTED_TIMESTAMP,
                deserialized.getLastNavigationCommittedTimestampMillis());
    }

    @SmallTest
    @Test
    public void testWebContentsStateBug_crbug_1220839() throws InterruptedException {
        PersistedTabDataConfiguration.setUseTestConfig(false);
        String url = mTestServer.getURL("/chrome/test/data/browsing_data/e.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url);
        final Semaphore semaphore = new Semaphore(0);
        // Saving serialized CriticalPersistedTabData ensures we get a direct ByteBuffer
        // which is assumed in the rest of Clank. See crbug.com/1220839 for more details.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
                CriticalPersistedTabData criticalPersistedTabData =
                        new CriticalPersistedTabData(tab, "", "", PARENT_ID, ROOT_ID, TIMESTAMP,
                                TabStateExtractor.getWebContentsState(tab), CONTENT_STATE_VERSION,
                                OPENER_APP_ID, THEME_COLOR, LAUNCH_TYPE_AT_CREATION, USER_AGENT_A,
                                LAST_NAVIGATION_COMMITTED_TIMESTAMP);
                PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                        CriticalPersistedTabData.class, tab.isIncognito());
                FilePersistedTabDataStorage persistedTabDataStorage =
                        new FilePersistedTabDataStorage();
                persistedTabDataStorage.save(tab.getId(), config.getId(), () -> {
                    Serializer<ByteBuffer> serializer = criticalPersistedTabData.getSerializer();
                    serializer.preSerialize();
                    return serializer.get();
                }, semaphore::release);
            }
        });
        semaphore.acquire();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
                PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                        CriticalPersistedTabData.class, tab.isIncognito());

                SerializedCriticalPersistedTabData serialized =
                        CriticalPersistedTabData.restore(tab.getId(), tab.isIncognito());
                CriticalPersistedTabData deserialized =
                        new CriticalPersistedTabData(tab, serialized);
                assertEquals(EXPECTED_TITLE,
                        deserialized.getWebContentsState().getDisplayTitleFromState());
                assertEquals(url, deserialized.getWebContentsState().getVirtualUrlFromState());
            }
        });
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testOpenerAppIdNull() {
        Tab tab = mockTab(TAB_ID, false);
        CriticalPersistedTabData criticalPersistedTabData =
                new CriticalPersistedTabData(tab, "", "", PARENT_ID, ROOT_ID, TIMESTAMP,
                        WEB_CONTENTS_STATE, CONTENT_STATE_VERSION, null, THEME_COLOR,
                        LAUNCH_TYPE_AT_CREATION, USER_AGENT_A, LAST_NAVIGATION_COMMITTED_TIMESTAMP);
        Serializer<ByteBuffer> serializer = criticalPersistedTabData.getSerializer();
        serializer.preSerialize();
        ByteBuffer serialized = serializer.get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        CriticalPersistedTabData deserialized =
                new CriticalPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        assertEquals(null, deserialized.getOpenerAppId());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testUrlDoesntTriggerSave() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setUrl(URL_A);
            assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setUrl(URL_A);
            assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setUrl(URL_B);
            assertEquals(URL_B, spyCriticalPersistedTabData.getUrl());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setUrl(URL_A);
            assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setUrl(null);
            Assert.assertNull(spyCriticalPersistedTabData.getUrl());
            verify(spyCriticalPersistedTabData, times(0)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testChangeInTitleDoesntTriggerSave() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setTitle(TITLE_A);
            assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setTitle(TITLE_A);
            assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setTitle(TITLE_B);
            assertEquals(TITLE_B, spyCriticalPersistedTabData.getTitle());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setTitle(TITLE_A);
            assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
            verify(spyCriticalPersistedTabData, times(0)).save();

            spyCriticalPersistedTabData.setTitle(null);
            Assert.assertNull(spyCriticalPersistedTabData.getTitle());
            verify(spyCriticalPersistedTabData, times(0)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testRootIdSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
            assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
            assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setRootId(ROOT_ID_B);
            assertEquals(ROOT_ID_B, spyCriticalPersistedTabData.getRootId());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
            assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
            verify(spyCriticalPersistedTabData, times(3)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testParentIdSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
            assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
            assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setParentId(PARENT_ID_B);
            assertEquals(PARENT_ID_B, spyCriticalPersistedTabData.getParentId());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
            assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
            verify(spyCriticalPersistedTabData, times(3)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testTimestampMillisSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_B);
            assertEquals(TIMESTAMP_B, spyCriticalPersistedTabData.getTimestampMillis());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
            verify(spyCriticalPersistedTabData, times(3)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testLastNavigationCommittedTimestampMillisSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setLastNavigationCommittedTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A,
                    spyCriticalPersistedTabData.getLastNavigationCommittedTimestampMillis());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setLastNavigationCommittedTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A,
                    spyCriticalPersistedTabData.getLastNavigationCommittedTimestampMillis());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setLastNavigationCommittedTimestampMillis(TIMESTAMP_B);
            assertEquals(TIMESTAMP_B,
                    spyCriticalPersistedTabData.getLastNavigationCommittedTimestampMillis());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setLastNavigationCommittedTimestampMillis(TIMESTAMP_A);
            assertEquals(TIMESTAMP_A,
                    spyCriticalPersistedTabData.getLastNavigationCommittedTimestampMillis());
            verify(spyCriticalPersistedTabData, times(3)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testLaunchTypeSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
            assertEquals(
                    TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
            assertEquals(
                    TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_B);
            assertEquals(
                    TAB_LAUNCH_TYPE_B, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
            assertEquals(
                    TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
            verify(spyCriticalPersistedTabData, times(3)).save();

            spyCriticalPersistedTabData.setLaunchTypeAtCreation(null);
            Assert.assertNull(spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
            verify(spyCriticalPersistedTabData, times(4)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testWebContentsStateSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
            assertEquals(WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
            assertEquals(WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_B);
            assertEquals(WEB_CONTENTS_STATE_B, spyCriticalPersistedTabData.getWebContentsState());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
            assertEquals(WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
            verify(spyCriticalPersistedTabData, times(3)).save();

            spyCriticalPersistedTabData.setWebContentsState(null);
            Assert.assertNull(spyCriticalPersistedTabData.getWebContentsState());
            verify(spyCriticalPersistedTabData, times(4)).save();
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testUserAgentSavedWhenNecessary() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            CriticalPersistedTabData spyCriticalPersistedTabData =
                    spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
            spyCriticalPersistedTabData.setUserAgent(USER_AGENT_A);
            assertEquals(USER_AGENT_A, spyCriticalPersistedTabData.getUserAgent());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setUserAgent(USER_AGENT_A);
            assertEquals(USER_AGENT_A, spyCriticalPersistedTabData.getUserAgent());
            verify(spyCriticalPersistedTabData, times(1)).save();

            spyCriticalPersistedTabData.setUserAgent(USER_AGENT_B);
            assertEquals(USER_AGENT_B, spyCriticalPersistedTabData.getUserAgent());
            verify(spyCriticalPersistedTabData, times(2)).save();

            spyCriticalPersistedTabData.setUserAgent(USER_AGENT_A);
            assertEquals(USER_AGENT_A, spyCriticalPersistedTabData.getUserAgent());
            verify(spyCriticalPersistedTabData, times(3)).save();
        }
    }

    @SmallTest
    @Test
    public void testConvertTabLaunchTypeToProtoLaunchType() {
        for (@TabLaunchType Integer tabLaunchType = 0; tabLaunchType < TabLaunchType.SIZE;
                tabLaunchType++) {
            CriticalPersistedTabData.getLaunchType(tabLaunchType);
        }
    }

    @SmallTest
    @Test
    public void testConvertProtoLaunchTypeToTabLaunchType() {
        for (int type = LaunchTypeAtCreation.SIZE;
                type < LaunchTypeAtCreation.names.length + LaunchTypeAtCreation.SIZE; type++) {
            if (type == LaunchTypeAtCreation.UNKNOWN) continue;
            CriticalPersistedTabData.getLaunchType(type);
        }
    }

    @SmallTest
    @Test
    public void testConvertTabUserAgentToProtoUserAgentType() {
        for (@TabUserAgent int tabUserAgent = 0; tabUserAgent <= TabUserAgent.SIZE;
                tabUserAgent++) {
            int flatBufferUserAgentType = CriticalPersistedTabData.getUserAgentType(tabUserAgent);
            Assert.assertNotEquals("TabUserAgent value is invalid.", flatBufferUserAgentType,
                    UserAgentType.USER_AGENT_UNKNOWN);
            if (tabUserAgent != TabUserAgent.SIZE) continue;
            assertEquals("TabUserAgent and ProtoUserAgentType should have the same size.",
                    flatBufferUserAgentType, UserAgentType.USER_AGENT_SIZE);
        }
    }

    @SmallTest
    @Test
    public void testConvertProtoUserAgentTypeToTabUserAgent() {
        for (int type = 0; type < UserAgentType.names.length; type++) {
            if (type == UserAgentType.USER_AGENT_UNKNOWN) continue;
            @TabUserAgent
            int tabUserAgent = CriticalPersistedTabData.getTabUserAgentType(type);
            Assert.assertNotNull("ProtoUserAgentType value is invalid.", tabUserAgent);
            if (type != UserAgentType.USER_AGENT_SIZE) continue;
            assertEquals("TabUserAgent and ProtoUserAgentType should have the same size.",
                    tabUserAgent, TabUserAgent.SIZE);
        }
    }

    @SmallTest
    @Test
    public void testFlatBufferValuesUnchanged() {
        // FlatBuffer enum values should not be changed as they are persisted across restarts.
        // Changing them would cause backward compatibility issues crbug.com/1286984.
        assertEquals(-2, LaunchTypeAtCreation.SIZE);
        assertEquals(-1, LaunchTypeAtCreation.UNKNOWN);
        assertEquals(0, LaunchTypeAtCreation.FROM_LINK);
        assertEquals(1, LaunchTypeAtCreation.FROM_EXTERNAL_APP);
        assertEquals(2, LaunchTypeAtCreation.FROM_CHROME_UI);
        assertEquals(3, LaunchTypeAtCreation.FROM_RESTORE);
        assertEquals(4, LaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND);
        assertEquals(5, LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND);
        assertEquals(6, LaunchTypeAtCreation.FROM_REPARENTING);
        assertEquals(7, LaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT);
        assertEquals(8, LaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION);
        assertEquals(9, LaunchTypeAtCreation.FROM_BROWSER_ACTIONS);
        assertEquals(10, LaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB);
        assertEquals(11, LaunchTypeAtCreation.FROM_STARTUP);
        assertEquals(12, LaunchTypeAtCreation.FROM_START_SURFACE);
        assertEquals(13, LaunchTypeAtCreation.FROM_TAB_GROUP_UI);
        assertEquals(14, LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP);
        assertEquals(15, LaunchTypeAtCreation.FROM_APP_WIDGET);
        assertEquals(16, LaunchTypeAtCreation.FROM_LONGPRESS_INCOGNITO);
        assertEquals(17, LaunchTypeAtCreation.FROM_RECENT_TABS);
        assertEquals(18, LaunchTypeAtCreation.FROM_READING_LIST);
        assertEquals(19, LaunchTypeAtCreation.FROM_TAB_SWITCHER_UI);
        assertEquals(20, LaunchTypeAtCreation.FROM_RESTORE_TABS_UI);
        assertEquals(21, LaunchTypeAtCreation.FROM_OMNIBOX);
        assertEquals("Need to increment 1 to expected value each time a LaunchTypeAtCreation "
                        + "is added. Also need to add any new LaunchTypeAtCreation to this test.",
                24, LaunchTypeAtCreation.names.length);
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testNullWebContentsState_1() {
        Tab tab = new MockTab(1, false);
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab,
                CriticalPersistedTabData.getMapperForTesting().map(
                        getFlatBufferWithNoWebContentsState()));
        assertEquals(0, criticalPersistedTabData.getWebContentsState().buffer().limit());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testNullWebContentsState_2() {
        Tab tab = new MockTab(1, false);
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab);
        criticalPersistedTabData.deserialize(getFlatBufferWithNoWebContentsState());
        assertEquals(0, criticalPersistedTabData.getWebContentsState().buffer().limit());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testExternalShouldSave() {
        try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
            MockPersistedTabDataStorage storage = new MockPersistedTabDataStorage();
            MockPersistedTabDataStorage spyStorage = spy(storage);
            TabImpl tab = new MockTab(1, false);
            CriticalPersistedTabData criticalPersistedTabData =
                    new CriticalPersistedTabData(tab, spyStorage, MOCK_DATA_ID);
            criticalPersistedTabData.setUrl(URL_A);
            // LIVE_IN_BACKGROUND ensures shouldSave is not set to true - see TabStateAttributes
            // initialization.
            tab.initialize(null, TabCreationState.LIVE_IN_BACKGROUND, null, null, null, false, null,
                    false);
            tab.getUserDataHost().setUserData(
                    CriticalPersistedTabData.class, criticalPersistedTabData);
            tab.registerTabSaving();
            tab.setIsTabSaveEnabled(true);

            // shouldSave flag not yet set, so save shouldn't go through
            criticalPersistedTabData.save();
            verify(spyStorage, times(0))
                    .save(anyInt(), anyString(), any(Serializer.class), any(Callback.class));

            // shouldSave set, so save should go through
            criticalPersistedTabData.setShouldSave();
            criticalPersistedTabData.save();
            verify(spyStorage, times(1))
                    .save(anyInt(), anyString(), any(Serializer.class), any(Callback.class));

            // shouldSave reset after recent save, so save shouldn't go through
            criticalPersistedTabData.save();
            verify(spyStorage, times(1))
                    .save(anyInt(), anyString(), any(Serializer.class), any(Callback.class));

            // shouldSave set again so save should go through
            criticalPersistedTabData.setShouldSave();
            criticalPersistedTabData.save();
            verify(spyStorage, times(2))
                    .save(anyInt(), anyString(), any(Serializer.class), any(Callback.class));
        }
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testSetRootIdUninitializedTab() {
        Tab uninitializedTab = new MockTab(1, false);
        Assert.assertFalse(uninitializedTab.isInitialized());
        CriticalPersistedTabData criticalPersistedTabData =
                new CriticalPersistedTabData(uninitializedTab);
        uninitializedTab.getUserDataHost().setUserData(
                CriticalPersistedTabData.class, criticalPersistedTabData);
        TabStateAttributes.createForTab(uninitializedTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(uninitializedTab).clearTabStateDirtiness();
        assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(uninitializedTab).getDirtinessState());
        criticalPersistedTabData.setRootId(ROOT_ID_A);
        assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(uninitializedTab).getDirtinessState());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testSetRootIdInitializedTab() {
        MockTab initializedTab = new MockTab(1, false);
        initializedTab.setIsInitialized(true);
        assertTrue(initializedTab.isInitialized());
        CriticalPersistedTabData criticalPersistedTabData =
                new CriticalPersistedTabData(initializedTab);
        initializedTab.getUserDataHost().setUserData(
                CriticalPersistedTabData.class, criticalPersistedTabData);
        TabStateAttributes.createForTab(initializedTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(initializedTab).clearTabStateDirtiness();
        assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(initializedTab).getDirtinessState());
        criticalPersistedTabData.setRootId(ROOT_ID_A);
        Assert.assertNotEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(initializedTab).getDirtinessState());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testCompatabilityChangeWithOldFlatBuffer() {
        FlatBufferBuilder fbb = new FlatBufferBuilder();
        int oaid = fbb.createString(OPENER_APP_ID);

        CriticalPersistedTabDataFlatBufferTest.startCriticalPersistedTabDataFlatBufferTest(fbb);
        CriticalPersistedTabDataFlatBufferTest.addParentId(fbb, PARENT_ID);
        CriticalPersistedTabDataFlatBufferTest.addRootId(fbb, ROOT_ID);
        CriticalPersistedTabDataFlatBufferTest.addTimestampMillis(fbb, TIMESTAMP);
        CriticalPersistedTabDataFlatBufferTest.addContentStateVersion(fbb, CONTENT_STATE_VERSION);
        CriticalPersistedTabDataFlatBufferTest.addOpenerAppId(fbb, oaid);
        CriticalPersistedTabDataFlatBufferTest.addThemeColor(fbb, THEME_COLOR);
        CriticalPersistedTabDataFlatBufferTest.addLaunchTypeAtCreation(
                fbb, LaunchTypeAtCreationTest.FROM_LINK);
        CriticalPersistedTabDataFlatBufferTest.addUserAgent(fbb, UserAgentTypeTest.DEFAULT);

        int r = CriticalPersistedTabDataFlatBufferTest.endCriticalPersistedTabDataFlatBufferTest(
                fbb);
        fbb.finish(r);

        ByteBuffer byteBuffer = fbb.dataBuffer();
        MockTab tab = new MockTab(TAB_ID, false);

        // Check de-serialization works.
        assertTrue(CriticalPersistedTabData.from(tab).deserialize(byteBuffer));

        CriticalPersistedTabData deserialized = CriticalPersistedTabData.from(tab);
        assertEquals(PARENT_ID, deserialized.getParentId());
        assertEquals(ROOT_ID, deserialized.getRootId());
        assertEquals(TIMESTAMP, deserialized.getTimestampMillis());
        assertEquals(CONTENT_STATE_VERSION, deserialized.getContentStateVersion());
        assertEquals(OPENER_APP_ID, deserialized.getOpenerAppId());
        assertEquals(THEME_COLOR, deserialized.getThemeColor());
        assertEquals(LaunchTypeAtCreationTest.FROM_LINK,
                (int) deserialized.getTabLaunchTypeAtCreation());
        assertEquals(TabUserAgent.DEFAULT, deserialized.getUserAgent());
    }

    private static final ByteBuffer getFlatBufferWithNoWebContentsState() {
        FlatBufferBuilder fbb = new FlatBufferBuilder();
        int oaid = fbb.createString(OPENER_APP_ID);
        CriticalPersistedTabDataFlatBuffer.startCriticalPersistedTabDataFlatBuffer(fbb);
        CriticalPersistedTabDataFlatBuffer.addParentId(fbb, PARENT_ID);
        CriticalPersistedTabDataFlatBuffer.addRootId(fbb, ROOT_ID);
        CriticalPersistedTabDataFlatBuffer.addTimestampMillis(fbb, TIMESTAMP);
        // WebContentsState intentionally left out
        CriticalPersistedTabDataFlatBuffer.addContentStateVersion(fbb, CONTENT_STATE_VERSION);
        CriticalPersistedTabDataFlatBuffer.addOpenerAppId(fbb, oaid);
        CriticalPersistedTabDataFlatBuffer.addThemeColor(fbb, THEME_COLOR);
        CriticalPersistedTabDataFlatBuffer.addLaunchTypeAtCreation(
                fbb, LaunchTypeAtCreation.FROM_LINK);
        CriticalPersistedTabDataFlatBuffer.addUserAgent(fbb, UserAgentType.DEFAULT);
        CriticalPersistedTabDataFlatBuffer.addLastNavigationCommittedTimestampMillis(
                fbb, LAST_NAVIGATION_COMMITTED_TIMESTAMP);

        int r = CriticalPersistedTabDataFlatBuffer.endCriticalPersistedTabDataFlatBuffer(fbb);
        fbb.finish(r);
        return fbb.dataBuffer();
    }
}
