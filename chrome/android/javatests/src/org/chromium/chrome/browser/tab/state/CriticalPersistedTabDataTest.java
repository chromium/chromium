// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.proto.CriticalPersistedTabData.CriticalPersistedTabDataProto;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;

/**
 * Test relating to {@link CriticalPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CriticalPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final int TAB_ID = 1;
    private static final int PARENT_ID = 2;
    private static final int ROOT_ID = 3;
    private static final int CONTENT_STATE_VERSION = 42;
    private static final byte[] WEB_CONTENTS_STATE_BYTES = {9, 10};
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(ByteBuffer.allocateDirect(WEB_CONTENTS_STATE_BYTES.length));
    private static final long TIMESTAMP = 203847028374L;
    private static final String APP_ID = "AppId";
    private static final String OPENER_APP_ID = "OpenerAppId";
    private static final int THEME_COLOR = 5;
    private static final Integer LAUNCH_TYPE_AT_CREATION = 3;
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

    static {
        WEB_CONTENTS_STATE.buffer().put(WEB_CONTENTS_STATE_BYTES);
        WEB_CONTENTS_STATE_A.buffer().put(WEB_CONTENTS_STATE_A_BYTES);
        WEB_CONTENTS_STATE_B.buffer().put(WEB_CONTENTS_STATE_B_BYTES);
    }

    private CriticalPersistedTabData mCriticalPersistedTabData;
    private MockPersistedTabDataStorage mStorage;

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
            CriticalPersistedTabData criticalPersistedTabData =
                    new CriticalPersistedTabData(mockTab(TAB_ID, isEncrypted), "", "", PARENT_ID,
                            ROOT_ID, TIMESTAMP, WEB_CONTENTS_STATE, CONTENT_STATE_VERSION,
                            OPENER_APP_ID, THEME_COLOR, LAUNCH_TYPE_AT_CREATION);
            criticalPersistedTabData.setShouldSaveForTesting(true);
            mStorage.setSemaphore(saveSemaphore);
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
            supplier.set(true);
            criticalPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
            criticalPersistedTabData.save();
            acquireSemaphore(saveSemaphore);
            CriticalPersistedTabData.from(mockTab(TAB_ID, isEncrypted), callback);
        });
        semaphore.acquire();
        Assert.assertNotNull(mCriticalPersistedTabData);
        Assert.assertEquals(mCriticalPersistedTabData.getParentId(), PARENT_ID);
        Assert.assertEquals(mCriticalPersistedTabData.getRootId(), ROOT_ID);
        Assert.assertEquals(mCriticalPersistedTabData.getTimestampMillis(), TIMESTAMP);
        Assert.assertEquals(
                mCriticalPersistedTabData.getContentStateVersion(), CONTENT_STATE_VERSION);
        Assert.assertEquals(mCriticalPersistedTabData.getOpenerAppId(), OPENER_APP_ID);
        Assert.assertEquals(mCriticalPersistedTabData.getThemeColor(), THEME_COLOR);
        Assert.assertEquals(
                mCriticalPersistedTabData.getTabLaunchTypeAtCreation(), LAUNCH_TYPE_AT_CREATION);
        Assert.assertArrayEquals(CriticalPersistedTabData.getContentStateByteArray(
                                         mCriticalPersistedTabData.getWebContentsState().buffer()),
                WEB_CONTENTS_STATE_BYTES);
        Semaphore deleteSemaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.setSemaphore(deleteSemaphore);
            mCriticalPersistedTabData.delete();
            acquireSemaphore(deleteSemaphore);
            CriticalPersistedTabData.from(mockTab(TAB_ID, isEncrypted), callback);
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

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveNTP() throws Throwable {
        for (boolean canGoBack : new boolean[] {false, true}) {
            for (boolean canGoForward : new boolean[] {false, true}) {
                CriticalPersistedTabData spyCriticalPersistedTabData =
                        prepareCPTDShouldTabSave(canGoBack, canGoForward);
                spyCriticalPersistedTabData.setUrl(new GURL(UrlConstants.NTP_URL));
                Assert.assertEquals(
                        canGoBack || canGoForward, spyCriticalPersistedTabData.shouldSave());
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveContentUrl() throws Throwable {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                prepareCPTDShouldTabSave(false, false);
        spyCriticalPersistedTabData.setUrl(new GURL("content://my_content"));
        Assert.assertFalse(spyCriticalPersistedTabData.shouldSave());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShouldTabSaveRegularUrl() throws Throwable {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                prepareCPTDShouldTabSave(false, false);
        spyCriticalPersistedTabData.setUrl(new GURL("https://www.google.com"));
        Assert.assertTrue(spyCriticalPersistedTabData.shouldSave());
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
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab, "",
                "", PARENT_ID, ROOT_ID, TIMESTAMP, WEB_CONTENTS_STATE, CONTENT_STATE_VERSION,
                OPENER_APP_ID, THEME_COLOR, LAUNCH_TYPE_AT_CREATION);
        byte[] serialized = criticalPersistedTabData.getSerializeSupplier().get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        CriticalPersistedTabData deserialized =
                new CriticalPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        Assert.assertNotNull(deserialized);
        Assert.assertEquals(PARENT_ID, deserialized.getParentId());
        Assert.assertEquals(ROOT_ID, deserialized.getRootId());
        Assert.assertEquals(TIMESTAMP, deserialized.getTimestampMillis());
        Assert.assertEquals(CONTENT_STATE_VERSION, deserialized.getContentStateVersion());
        Assert.assertEquals(OPENER_APP_ID, deserialized.getOpenerAppId());
        Assert.assertEquals(THEME_COLOR, deserialized.getThemeColor());
        Assert.assertEquals(LAUNCH_TYPE_AT_CREATION, deserialized.getTabLaunchTypeAtCreation());
        Assert.assertArrayEquals(WEB_CONTENTS_STATE_BYTES,
                CriticalPersistedTabData.getContentStateByteArray(
                        deserialized.getWebContentsState().buffer()));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testOpenerAppIdNull() {
        Tab tab = mockTab(TAB_ID, false);
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab, "",
                "", PARENT_ID, ROOT_ID, TIMESTAMP, WEB_CONTENTS_STATE, CONTENT_STATE_VERSION, null,
                THEME_COLOR, LAUNCH_TYPE_AT_CREATION);
        byte[] serialized = criticalPersistedTabData.getSerializeSupplier().get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        CriticalPersistedTabData deserialized =
                new CriticalPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        Assert.assertEquals(null, deserialized.getOpenerAppId());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testUrlSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setUrl(URL_A);
        Assert.assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setUrl(URL_A);
        Assert.assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setUrl(URL_B);
        Assert.assertEquals(URL_B, spyCriticalPersistedTabData.getUrl());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setUrl(URL_A);
        Assert.assertEquals(URL_A, spyCriticalPersistedTabData.getUrl());
        verify(spyCriticalPersistedTabData, times(3)).save();

        spyCriticalPersistedTabData.setUrl(null);
        Assert.assertNull(spyCriticalPersistedTabData.getUrl());
        verify(spyCriticalPersistedTabData, times(4)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testTitleSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setTitle(TITLE_A);
        Assert.assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setTitle(TITLE_A);
        Assert.assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setTitle(TITLE_B);
        Assert.assertEquals(TITLE_B, spyCriticalPersistedTabData.getTitle());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setTitle(TITLE_A);
        Assert.assertEquals(TITLE_A, spyCriticalPersistedTabData.getTitle());
        verify(spyCriticalPersistedTabData, times(3)).save();

        spyCriticalPersistedTabData.setTitle(null);
        Assert.assertNull(spyCriticalPersistedTabData.getTitle());
        verify(spyCriticalPersistedTabData, times(4)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testRootIdSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
        Assert.assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
        Assert.assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setRootId(ROOT_ID_B);
        Assert.assertEquals(ROOT_ID_B, spyCriticalPersistedTabData.getRootId());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setRootId(ROOT_ID_A);
        Assert.assertEquals(ROOT_ID_A, spyCriticalPersistedTabData.getRootId());
        verify(spyCriticalPersistedTabData, times(3)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testParentIdSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
        Assert.assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
        Assert.assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setParentId(PARENT_ID_B);
        Assert.assertEquals(PARENT_ID_B, spyCriticalPersistedTabData.getParentId());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setParentId(PARENT_ID_A);
        Assert.assertEquals(PARENT_ID_A, spyCriticalPersistedTabData.getParentId());
        verify(spyCriticalPersistedTabData, times(3)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testTimestampMillisSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
        Assert.assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
        Assert.assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_B);
        Assert.assertEquals(TIMESTAMP_B, spyCriticalPersistedTabData.getTimestampMillis());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setTimestampMillis(TIMESTAMP_A);
        Assert.assertEquals(TIMESTAMP_A, spyCriticalPersistedTabData.getTimestampMillis());
        verify(spyCriticalPersistedTabData, times(3)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testLaunchTypeSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
        Assert.assertEquals(
                TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
        Assert.assertEquals(
                TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_B);
        Assert.assertEquals(
                TAB_LAUNCH_TYPE_B, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setLaunchTypeAtCreation(TAB_LAUNCH_TYPE_A);
        Assert.assertEquals(
                TAB_LAUNCH_TYPE_A, spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
        verify(spyCriticalPersistedTabData, times(3)).save();

        spyCriticalPersistedTabData.setLaunchTypeAtCreation(null);
        Assert.assertNull(spyCriticalPersistedTabData.getTabLaunchTypeAtCreation());
        verify(spyCriticalPersistedTabData, times(4)).save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testWebContentsStateSavedWhenNecessary() {
        CriticalPersistedTabData spyCriticalPersistedTabData =
                spy(CriticalPersistedTabData.from(mockTab(TAB_ID, false)));
        spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
        Assert.assertEquals(
                WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
        Assert.assertEquals(
                WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
        verify(spyCriticalPersistedTabData, times(1)).save();

        spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_B);
        Assert.assertEquals(
                WEB_CONTENTS_STATE_B, spyCriticalPersistedTabData.getWebContentsState());
        verify(spyCriticalPersistedTabData, times(2)).save();

        spyCriticalPersistedTabData.setWebContentsState(WEB_CONTENTS_STATE_A);
        Assert.assertEquals(
                WEB_CONTENTS_STATE_A, spyCriticalPersistedTabData.getWebContentsState());
        verify(spyCriticalPersistedTabData, times(3)).save();

        spyCriticalPersistedTabData.setWebContentsState(null);
        Assert.assertNull(spyCriticalPersistedTabData.getWebContentsState());
        verify(spyCriticalPersistedTabData, times(4)).save();
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
        for (CriticalPersistedTabDataProto.LaunchTypeAtCreation type :
                CriticalPersistedTabDataProto.LaunchTypeAtCreation.values()) {
            if (type == CriticalPersistedTabDataProto.LaunchTypeAtCreation.UNKNOWN) continue;
            CriticalPersistedTabData.getLaunchType(type);
        }
    }
}
