// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.JournalMutation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link FeedJournalStorage}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedJournalStorageTest {
    public static final String JOURNAL_KEY1 = "JOURNAL_KEY_1";
    public static final String JOURNAL_KEY2 = "JOURNAL_KEY_2";
    public static final String JOURNAL_KEY3 = "JOURNAL_KEY_3";
    public static final byte[] JOURNAL_DATA1 = {24, -119, -10, -71, -35, 5};
    public static final byte[] JOURNAL_DATA2 = {8, 3, 18, 53, 70, 69, 65, 84, 85, 82, 69};
    public static final byte[] JOURNAL_DATA3 = {54, 55, 56, 48, 51, 57, 24, -119, -10};

    @Mock
    private FeedJournalBridge mBridge;
    @Mock
    private Consumer<CommitResult> mCommitResultConsumer;
    @Mock
    private Consumer<Result<Boolean>> mBooleanConsumer;
    @Mock
            private Consumer < Result < List<byte[]>>> mListOfByteArrayConsumer;
    @Mock
            private Consumer < Result < List<String>>> mListOfStringConsumer;
    @Mock
    private Profile mProfile;
    @Captor
    private ArgumentCaptor<CommitResult> mCommitResultCaptor;
    @Captor
    private ArgumentCaptor<Result<Boolean>> mBooleanCaptor;
    @Captor
            private ArgumentCaptor < Result < List<String>>> mListOfStringCaptor;
    @Captor
            private ArgumentCaptor < Result < List<byte[]>>> mListOfByteArrayCaptor;
    @Captor
    private ArgumentCaptor<String> mStringArgument;
    @Captor
    private ArgumentCaptor<Callback<Boolean>> mBooleanSuccessCallbackArgument;
    @Captor
    private ArgumentCaptor<Callback<byte[][]>> mArrayOfByteArraySuccessCallbackArgument;
    @Captor
    private ArgumentCaptor<Callback<String[]>> mStringArraySuccessCallbackArgument;
    @Captor
    private ArgumentCaptor<Callback<Void>> mFailureCallbackArgument;
    @Captor
    private ArgumentCaptor<JournalMutation> mJournalMutationArgument;

    private FeedJournalStorage mJournalStorage;

    private Answer<Void> createArrayOfByteArraySuccessAnswer(byte[][] arrayOfByteArray) {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mArrayOfByteArraySuccessCallbackArgument.getValue().onResult(arrayOfByteArray);
                return null;
            }
        };
    }

    private Answer<Void> createStringArraySuccessAnswer(String[] stringArray) {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mStringArraySuccessCallbackArgument.getValue().onResult(stringArray);
                return null;
            }
        };
    }

    private Answer<Void> createFailureAnswer() {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mFailureCallbackArgument.getValue().onResult(null);
                return null;
            }
        };
    }

    private Answer<Void> createBooleanSuccessAnswer(Boolean bool) {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mBooleanSuccessCallbackArgument.getValue().onResult(bool);
                return null;
            }
        };
    }

    private void verifyListOfBytesResult(
            List<byte[]> expectedList, Result<List<byte[]>> actualResult) {
        List<byte[]> actualList = actualResult.getValue();
        assertEquals(expectedList.size(), actualList.size());
        for (byte[] actualString : actualList) {
            boolean foundSame = false;
            for (byte[] expectedBytes : expectedList) {
                if (Arrays.equals(expectedBytes, actualString)) {
                    foundSame = true;
                    break;
                }
            }
            assertTrue(foundSame);
        }
    }

    private void verifyListOfStringResult(
            String[] expectedStrings, boolean expectedSuccess, Result<List<String>> actualResult) {
        assertEquals(expectedSuccess, actualResult.isSuccessful());
        if (!expectedSuccess) return;

        List<String> actualList = actualResult.getValue();
        assertEquals(expectedStrings.length, actualList.size());
        for (String expectedString : expectedStrings) {
            assertTrue(actualList.contains(expectedString));
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mJournalStorage = new FeedJournalStorage(mBridge);
    }

    @Test
    @SmallTest
    public void readTest() {
        byte[][] answerStrings = {JOURNAL_DATA1, JOURNAL_DATA2, JOURNAL_DATA3};
        Answer<Void> answer = createArrayOfByteArraySuccessAnswer(answerStrings);
        doAnswer(answer).when(mBridge).loadJournal(mStringArgument.capture(),
                mArrayOfByteArraySuccessCallbackArgument.capture(),
                mFailureCallbackArgument.capture());

        mJournalStorage.read(JOURNAL_KEY1, mListOfByteArrayConsumer);
        verify(mBridge, times(1))
                .loadJournal(eq(JOURNAL_KEY1), mArrayOfByteArraySuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mListOfByteArrayConsumer, times(1)).accept(mListOfByteArrayCaptor.capture());
        assertEquals(true, mListOfByteArrayCaptor.getValue().isSuccessful());
        List<byte[]> expectedStrings = new ArrayList<byte[]>();
        expectedStrings.add(JOURNAL_DATA1);
        expectedStrings.add(JOURNAL_DATA2);
        expectedStrings.add(JOURNAL_DATA3);
        verifyListOfBytesResult(expectedStrings, mListOfByteArrayCaptor.getValue());
    }

    @Test
    @SmallTest
    public void readFailureTest() {
        byte[][] answerStrings = {};
        Answer<Void> answer = createFailureAnswer();
        doAnswer(answer).when(mBridge).loadJournal(mStringArgument.capture(),
                mArrayOfByteArraySuccessCallbackArgument.capture(),
                mFailureCallbackArgument.capture());

        mJournalStorage.read(JOURNAL_KEY1, mListOfByteArrayConsumer);
        verify(mBridge, times(1))
                .loadJournal(eq(JOURNAL_KEY1), mArrayOfByteArraySuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mListOfByteArrayConsumer, times(1)).accept(mListOfByteArrayCaptor.capture());
        assertEquals(false, mListOfByteArrayCaptor.getValue().isSuccessful());
    }

    @Test
    @SmallTest
    public void existsTest() {
        Answer<Void> answer = createBooleanSuccessAnswer(true);
        doAnswer(answer).when(mBridge).doesJournalExist(mStringArgument.capture(),
                mBooleanSuccessCallbackArgument.capture(), mFailureCallbackArgument.capture());

        mJournalStorage.exists(JOURNAL_KEY1, mBooleanConsumer);
        verify(mBridge, times(1))
                .doesJournalExist(eq(JOURNAL_KEY1), mBooleanSuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mBooleanConsumer, times(1)).accept(mBooleanCaptor.capture());
        assertTrue(mBooleanCaptor.getValue().getValue());
    }

    @Test
    @SmallTest
    public void existsFailureTest() {
        Answer<Void> answer = createFailureAnswer();
        doAnswer(answer).when(mBridge).doesJournalExist(mStringArgument.capture(),
                mBooleanSuccessCallbackArgument.capture(), mFailureCallbackArgument.capture());

        mJournalStorage.exists(JOURNAL_KEY1, mBooleanConsumer);
        verify(mBridge, times(1))
                .doesJournalExist(eq(JOURNAL_KEY1), mBooleanSuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mBooleanConsumer, times(1)).accept(mBooleanCaptor.capture());
        assertFalse(mBooleanCaptor.getValue().isSuccessful());
    }

    @Test
    @SmallTest
    public void getAllJournalsTest() {
        String[] answerStrings = {JOURNAL_KEY1, JOURNAL_KEY2, JOURNAL_KEY3};
        Answer<Void> answer = createStringArraySuccessAnswer(answerStrings);
        doAnswer(answer).when(mBridge).loadAllJournalKeys(
                mStringArraySuccessCallbackArgument.capture(), mFailureCallbackArgument.capture());

        mJournalStorage.getAllJournals(mListOfStringConsumer);
        verify(mBridge, times(1))
                .loadAllJournalKeys(mStringArraySuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mListOfStringConsumer, times(1)).accept(mListOfStringCaptor.capture());
        verifyListOfStringResult(answerStrings, true, mListOfStringCaptor.getValue());
    }

    @Test
    @SmallTest
    public void getAllJournalsFailureTest() {
        List<String> answerStrings = new ArrayList<String>();
        Answer<Void> answer = createFailureAnswer();
        doAnswer(answer).when(mBridge).loadAllJournalKeys(
                mStringArraySuccessCallbackArgument.capture(), mFailureCallbackArgument.capture());

        mJournalStorage.getAllJournals(mListOfStringConsumer);
        verify(mBridge, times(1))
                .loadAllJournalKeys(mStringArraySuccessCallbackArgument.capture(),
                        mFailureCallbackArgument.capture());
        verify(mListOfStringConsumer, times(1)).accept(mListOfStringCaptor.capture());
        assertFalse(mListOfStringCaptor.getValue().isSuccessful());
    }

    @Test
    @SmallTest
    public void deleteAllTest() {
        Answer<Void> answer = createBooleanSuccessAnswer(true);
        doAnswer(answer).when(mBridge).deleteAllJournals(mBooleanSuccessCallbackArgument.capture());

        mJournalStorage.deleteAll(mCommitResultConsumer);
        verify(mBridge, times(1)).deleteAllJournals(mBooleanSuccessCallbackArgument.capture());
        verify(mCommitResultConsumer, times(1)).accept(mCommitResultCaptor.capture());
        CommitResult commitResult = mCommitResultCaptor.getValue();
        assertEquals(CommitResult.SUCCESS, commitResult);
    }

    @Test
    @SmallTest
    public void commitTest() {
        Answer<Void> answerCommitJournal = createBooleanSuccessAnswer(true);
        doAnswer(answerCommitJournal)
                .when(mBridge)
                .commitJournalMutation(mJournalMutationArgument.capture(),
                        mBooleanSuccessCallbackArgument.capture());

        mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_KEY1)
                                       .append(JOURNAL_DATA1)
                                       .copy(JOURNAL_KEY2)
                                       .delete()
                                       .build(),
                mCommitResultConsumer);
        verify(mBridge, times(1))
                .commitJournalMutation(mJournalMutationArgument.capture(),
                        mBooleanSuccessCallbackArgument.capture());

        verify(mCommitResultConsumer, times(1)).accept(mCommitResultCaptor.capture());
        CommitResult commitResult = mCommitResultCaptor.getValue();
        assertEquals(CommitResult.SUCCESS, commitResult);
    }
}
