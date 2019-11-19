// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for OfflinePageUtils.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class})
public class OfflinePageBridgeUnitTest {
    private OfflinePageBridge mBridge;

    private static final String TEST_NAMESPACE = "TEST_NAMESPACE";
    private static final String TEST_ID = "TEST_ID";
    private static final String TEST_URL = "TEST_URL";
    private static final long TEST_OFFLINE_ID = 42;
    private static final ClientId TEST_CLIENT_ID = new ClientId(TEST_NAMESPACE, TEST_ID);
    private static final String TEST_FILE_PATH = "TEST_FILE_PATH";
    private static final long TEST_FILESIZE = 12345;
    private static final long TEST_CREATIONTIMEMS = 150;
    private static final int TEST_ACCESSCOUNT = 1;
    private static final long TEST_LASTACCESSTIMEMS = 20160314;
    private static final String TEST_REQUEST_ORIGIN = "abc.xyz";

    private static final OfflinePageItem TEST_OFFLINE_PAGE_ITEM = new OfflinePageItem(TEST_URL,
            TEST_OFFLINE_ID, TEST_NAMESPACE, TEST_ID, "" /* title */, TEST_FILE_PATH, TEST_FILESIZE,
            TEST_CREATIONTIMEMS, TEST_ACCESSCOUNT, TEST_LASTACCESSTIMEMS, TEST_REQUEST_ORIGIN);

    @Captor
    ArgumentCaptor<List<OfflinePageItem>> mResultArgument;

    @Captor
    ArgumentCaptor<Callback<List<OfflinePageItem>>> mCallbackArgument;

    @Captor
    ArgumentCaptor<String[]> mNamespacesArgument;

    @Captor
    ArgumentCaptor<String[]> mIdsArgument;

    @Captor
    ArgumentCaptor<long[]> mOfflineIdsArgument;

    @Captor
    ArgumentCaptor<Callback<Integer>> mDeleteCallbackArgument;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    OfflinePageBridge.Natives mOfflinePageBridgeJniMock;

    /**
     * Mocks the observer.
     */
    public class MockOfflinePageModelObserver extends OfflinePageModelObserver {
        public long lastDeletedOfflineId;
        public ClientId lastDeletedClientId;

        @Override
        public void offlinePageDeleted(DeletedPageInfo deletedPage) {
            lastDeletedOfflineId = deletedPage.getOfflineId();
            lastDeletedClientId = deletedPage.getClientId();
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(OfflinePageBridgeJni.TEST_HOOKS, mOfflinePageBridgeJniMock);
        OfflinePageBridge bridge = new OfflinePageBridge(0);
        // Using the spy to automatically marshal all the calls to the original methods if they are
        // not mocked explicitly.
        mBridge = spy(bridge);
    }

    /**
     * Tests OfflinePageBridge#OfflinePageDeleted() callback with two observers attached.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testRemovePageByClientId() {
        MockOfflinePageModelObserver observer1 = new MockOfflinePageModelObserver();
        MockOfflinePageModelObserver observer2 = new MockOfflinePageModelObserver();
        mBridge.addObserver(observer1);
        mBridge.addObserver(observer2);

        ClientId testClientId = new ClientId(TEST_NAMESPACE, TEST_ID);
        long testOfflineId = 123;
        mBridge.offlinePageDeleted(new DeletedPageInfo(testOfflineId, testClientId, ""));
        assertEquals(testOfflineId, observer1.lastDeletedOfflineId);
        assertEquals(testClientId, observer1.lastDeletedClientId);
        assertEquals(testOfflineId, observer2.lastDeletedOfflineId);
        assertEquals(testClientId, observer2.lastDeletedClientId);
    }

    /**
     * Tests OfflinePageBridge#GetAllPages() callback when there are no pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetAllPages_listOfPagesEmpty() {
        final int itemCount = 0;

        answerNativeGetAllPages(itemCount);
        Callback<List<OfflinePageItem>> callback = createMultipleItemCallback(itemCount);
        mBridge.getAllPages(callback);

        List<OfflinePageItem> itemList = new ArrayList<OfflinePageItem>();
        verify(callback, times(1)).onResult(itemList);
    }

    /**
     * Tests OfflinePageBridge#GetAllPages() callback when there are pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetAllPages_listOfPagesNonEmpty() {
        final int itemCount = 2;

        answerNativeGetAllPages(itemCount);
        Callback<List<OfflinePageItem>> callback = createMultipleItemCallback(itemCount);
        mBridge.getAllPages(callback);

        List<OfflinePageItem> itemList = new ArrayList<OfflinePageItem>();
        itemList.add(TEST_OFFLINE_PAGE_ITEM);
        itemList.add(TEST_OFFLINE_PAGE_ITEM);
        verify(callback, times(1)).onResult(itemList);
    }

    /**
     * Tests OfflinePageBridge#GetPagesByClientIds() callback when there are no pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetPagesByClientIds_listOfClientIdsEmpty() {
        final int itemCount = 0;

        answerGetPagesByClientIds(itemCount);
        Callback<List<OfflinePageItem>> callback = createMultipleItemCallback(itemCount);
        ClientId secondClientId = new ClientId(TEST_NAMESPACE, "id number two");
        List<ClientId> list = new ArrayList<>();
        mBridge.getPagesByClientIds(list, callback);

        List<OfflinePageItem> itemList = new ArrayList<OfflinePageItem>();
        verify(callback, times(1)).onResult(itemList);
    }

    /**
     * Tests OfflinePageBridge#GetPagesByClientIds() callback when there are pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetPagesByClientIds() {
        final int itemCount = 2;

        answerGetPagesByClientIds(itemCount);
        Callback<List<OfflinePageItem>> callback = createMultipleItemCallback(itemCount);
        ClientId secondClientId = new ClientId(TEST_NAMESPACE, "id number two");
        List<ClientId> list = new ArrayList<>();
        list.add(TEST_CLIENT_ID);
        list.add(secondClientId);
        mBridge.getPagesByClientIds(list, callback);

        List<OfflinePageItem> itemList = new ArrayList<OfflinePageItem>();
        itemList.add(TEST_OFFLINE_PAGE_ITEM);
        itemList.add(TEST_OFFLINE_PAGE_ITEM);
        verify(callback, times(1)).onResult(itemList);
    }

    /**
     * Tests OfflinePageBridge#DeletePagesByClientIds() callback when there are no pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testDeletePagesByClientIds_listOfClientIdsEmpty() {
        final int itemCount = 0;

        answerDeletePagesByClientIds(itemCount);
        Callback<Integer> callback = createDeletePageCallback();
        ClientId secondClientId = new ClientId(TEST_NAMESPACE, "id number two");
        List<ClientId> list = new ArrayList<>();
        mBridge.deletePagesByClientId(list, callback);

        verify(callback, times(1)).onResult(any(Integer.class));
    }

    /**
     * Tests OfflinePageBridge#DeletePagesByClientIds() callback when there are pages.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testDeletePagesByClientIds() {
        final int itemCount = 2;

        answerDeletePagesByClientIds(itemCount);
        Callback<Integer> callback = createDeletePageCallback();
        ClientId secondClientId = new ClientId(TEST_NAMESPACE, "id number two");
        List<ClientId> list = new ArrayList<>();
        list.add(TEST_CLIENT_ID);
        list.add(secondClientId);
        mBridge.deletePagesByClientId(list, callback);

        verify(callback, times(1)).onResult(any(Integer.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDeletePagesByOfflineIds_listOfOfflineIdsNull() {
        // -1 means to check for null in the Answer.
        final int itemCount = -1;

        answerDeletePagesByOfflineIds(itemCount);
        Callback<Integer> callback = createDeletePageCallback();
        List<Long> list = null;

        mBridge.deletePagesByOfflineId(list, callback);

        verify(callback, times(1)).onResult(any(Integer.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDeletePagesByOfflineIds_listOfOfflineIdsEmpty() {
        final int itemCount = 0;

        answerDeletePagesByOfflineIds(itemCount);
        Callback<Integer> callback = createDeletePageCallback();
        List<Long> list = new ArrayList<>();

        mBridge.deletePagesByOfflineId(list, callback);

        verify(callback, times(1)).onResult(any(Integer.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDeletePagesByOfflineIds() {
        final int itemCount = 2;

        answerDeletePagesByOfflineIds(itemCount);
        Callback<Integer> callback = createDeletePageCallback();
        List<Long> list = new ArrayList<>();
        list.add(Long.valueOf(1));
        list.add(Long.valueOf(2));

        mBridge.deletePagesByOfflineId(list, callback);

        verify(callback, times(1)).onResult(any(Integer.class));
    }

    /** Performs a proper cast from Object to a List<OfflinePageItem>. */
    private static List<OfflinePageItem> convertToListOfOfflinePages(Object o) {
        @SuppressWarnings("unchecked")
        List<OfflinePageItem> list = (List<OfflinePageItem>) o;
        return list;
    }

    private Callback<List<OfflinePageItem>> createMultipleItemCallback(final int itemCount) {
        return spy(new Callback<List<OfflinePageItem>>() {
            @Override
            public void onResult(List<OfflinePageItem> items) {
                assertNotNull(items);
                assertEquals(itemCount, items.size());
            }
        });
    }

    private Callback<Integer> createDeletePageCallback() {
        return spy(new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {}
        });
    }

    private void answerNativeGetAllPages(final int itemCount) {
        Answer<Void> answer = new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                List<OfflinePageItem> result = mResultArgument.getValue();
                for (int i = 0; i < itemCount; i++) {
                    result.add(TEST_OFFLINE_PAGE_ITEM);
                }

                mCallbackArgument.getValue().onResult(result);

                return null;
            }
        };
        doAnswer(answer)
                .when(mOfflinePageBridgeJniMock)
                .getAllPages(anyLong(), eq(mBridge), mResultArgument.capture(),
                        mCallbackArgument.capture());
    }

    private void answerGetPagesByClientIds(final int itemCount) {
        Answer<Void> answer = new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                List<OfflinePageItem> result = mResultArgument.getValue();
                String[] namespaces = mNamespacesArgument.getValue();
                String[] ids = mIdsArgument.getValue();

                assertEquals(namespaces.length, itemCount);
                assertEquals(ids.length, itemCount);

                for (int i = 0; i < itemCount; i++) {
                    result.add(TEST_OFFLINE_PAGE_ITEM);
                }

                mCallbackArgument.getValue().onResult(result);

                return null;
            }
        };

        doAnswer(answer)
                .when(mOfflinePageBridgeJniMock)
                .getPagesByClientId(anyLong(), eq(mBridge), mResultArgument.capture(),
                        mNamespacesArgument.capture(), mIdsArgument.capture(),
                        mCallbackArgument.capture());
    }

    private void answerDeletePagesByOfflineIds(final int itemCount) {
        Answer<Void> answer = new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                long[] offlineIds = mOfflineIdsArgument.getValue();

                if (itemCount < 0) {
                    assertEquals(offlineIds, null);
                } else {
                    assertEquals(offlineIds.length, itemCount);
                }
                mDeleteCallbackArgument.getValue().onResult(Integer.valueOf(0));

                return null;
            }
        };

        doAnswer(answer)
                .when(mOfflinePageBridgeJniMock)
                .deletePagesByOfflineId(anyLong(), eq(mBridge), mOfflineIdsArgument.capture(),
                        mDeleteCallbackArgument.capture());
    }

    private void answerDeletePagesByClientIds(final int itemCount) {
        Answer<Void> answer = new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                String[] namespaces = mNamespacesArgument.getValue();
                String[] ids = mIdsArgument.getValue();

                assertEquals(namespaces.length, itemCount);
                assertEquals(ids.length, itemCount);

                mDeleteCallbackArgument.getValue().onResult(Integer.valueOf(0));

                return null;
            }
        };

        doAnswer(answer)
                .when(mOfflinePageBridgeJniMock)
                .deletePagesByClientId(anyLong(), eq(mBridge), mNamespacesArgument.capture(),
                        mIdsArgument.capture(), mDeleteCallbackArgument.capture());
    }
}
