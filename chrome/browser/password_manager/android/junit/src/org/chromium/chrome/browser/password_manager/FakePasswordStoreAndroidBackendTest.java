// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.hasItem;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.hasEntry;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.fail;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.PasswordWithLocalData;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.sync.protocol.PasswordSpecificsData;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.TimeoutException;

/** Tests for {@link FakePasswordStoreAndroidBackend}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class FakePasswordStoreAndroidBackendTest {
    private FakePasswordStoreAndroidBackend mBackend;

    private static final PasswordSpecificsData sPasswordData =
            PasswordSpecificsData.newBuilder()
                    .setUsernameValue("Todd Tester")
                    .setUsernameElement("username")
                    .setPasswordElement("pwd")
                    .setOrigin("https://accounts.google.com/signin")
                    .setSignonRealm("https://accounts.google.com")
                    .setPasswordValue("password")
                    .build();
    private static final PasswordSpecificsData sPasswordDataBlocklisted =
            PasswordSpecificsData.newBuilder()
                    .setUsernameElement("username")
                    .setPasswordElement("pwd")
                    .setOrigin("https://accounts.google.com/signin")
                    .setSignonRealm("https://accounts.google.com")
                    .setPasswordValue("password")
                    .setBlacklisted(true)
                    .build();
    private static final PasswordSpecificsData sPasswordDataNoOrigin =
            PasswordSpecificsData.newBuilder()
                    .setUsernameValue("Todd Tester")
                    .setUsernameElement("username")
                    .setPasswordElement("pwd")
                    .setSignonRealm("https://www.google.com")
                    .setPasswordValue("password")
                    .build();

    private static final PasswordWithLocalData sPwdWithLocalData =
            PasswordWithLocalData.newBuilder().setPasswordSpecificsData(sPasswordData).build();
    private static final PasswordWithLocalData sPwdWithLocalDataBlocklisted =
            PasswordWithLocalData.newBuilder()
                    .setPasswordSpecificsData(sPasswordDataBlocklisted)
                    .build();
    private static final PasswordWithLocalData sPwdWithLocalDataNoOrigin =
            PasswordWithLocalData.newBuilder()
                    .setPasswordSpecificsData(sPasswordDataNoOrigin)
                    .build();
    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));

    @Before
    public void setUp() {
        mBackend = new FakePasswordStoreAndroidBackend();
        mBackend.setSyncingAccount(sTestAccount.get());
    }

    @Test
    public void testAddLogin() throws TimeoutException {
        CallbackHelper successCallback = new CallbackHelper();

        mBackend.addLogin(
                sPwdWithLocalData.toByteArray(),
                sTestAccount,
                successCallback::notifyCalled,
                unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Map<Account, List<PasswordWithLocalData>> allPasswords = mBackend.getAllSavedPasswords();
        assertThat(successCallback.getCallCount(), is(1));
        assertThat(allPasswords.get(sTestAccount.get()), hasSize(1));
        assertThat(allPasswords, hasEntry(is(sTestAccount.get()), hasItem(sPwdWithLocalData)));
    }

    @Test
    public void testGetAllLogins() throws TimeoutException {
        fillPasswordStore();

        PayloadCallbackHelper<byte[]> successCallback = new PayloadCallbackHelper<>();
        mBackend.getAllLogins(sTestAccount, successCallback::notifyCalled, unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ListPasswordsResult actualPasswords =
                parseListPasswordResultOrFail(successCallback.getOnlyPayloadBlocking());
        ListPasswordsResult expectedPasswords =
                ListPasswordsResult.newBuilder()
                        .addPasswordData(sPwdWithLocalData)
                        .addPasswordData(sPwdWithLocalDataBlocklisted)
                        .addPasswordData(sPwdWithLocalDataNoOrigin)
                        .build();
        assertThat(actualPasswords, is(expectedPasswords));
    }

    @Test
    public void testGetAutofillableLogins() throws TimeoutException {
        fillPasswordStore();

        PayloadCallbackHelper<byte[]> successCallback = new PayloadCallbackHelper<>();
        mBackend.getAutofillableLogins(
                sTestAccount, successCallback::notifyCalled, unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ListPasswordsResult actualPasswords =
                parseListPasswordResultOrFail(successCallback.getOnlyPayloadBlocking());
        ListPasswordsResult expectedPasswords =
                ListPasswordsResult.newBuilder()
                        .addPasswordData(sPwdWithLocalData)
                        .addPasswordData(sPwdWithLocalDataNoOrigin)
                        .build();
        assertThat(actualPasswords, is(expectedPasswords));
    }

    @Test
    public void testGetLoginsForSignonRealm() throws TimeoutException {
        fillPasswordStore();

        PayloadCallbackHelper<byte[]> successCallback = new PayloadCallbackHelper<>();
        mBackend.getLoginsForSignonRealm(
                sPwdWithLocalData.getPasswordSpecificsData().getSignonRealm(),
                sTestAccount,
                successCallback::notifyCalled,
                unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ListPasswordsResult actualPasswords =
                parseListPasswordResultOrFail(successCallback.getOnlyPayloadBlocking());
        ListPasswordsResult expectedPasswords =
                ListPasswordsResult.newBuilder()
                        .addPasswordData(sPwdWithLocalData)
                        .addPasswordData(sPwdWithLocalDataBlocklisted)
                        .build();
        assertThat(actualPasswords, is(expectedPasswords));
    }

    @Test
    public void testUpdateLoginReplacesExisting() throws TimeoutException {
        fillPasswordStore();

        CallbackHelper successCallback = new CallbackHelper();
        PasswordSpecificsData updatedPasswordData =
                PasswordSpecificsData.newBuilder()
                        .setUsernameValue("Todd Tester")
                        .setUsernameElement("username")
                        .setPasswordElement("pwd")
                        .setOrigin("https://accounts.google.com/signin")
                        .setSignonRealm("https://accounts.google.com")
                        .setPasswordValue("UpdatedPassword")
                        .build();
        PasswordWithLocalData updatedPwdWithLocalData =
                PasswordWithLocalData.newBuilder()
                        .setPasswordSpecificsData(updatedPasswordData)
                        .build();
        mBackend.updateLogin(
                updatedPwdWithLocalData.toByteArray(),
                sTestAccount,
                successCallback::notifyCalled,
                unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Map<Account, List<PasswordWithLocalData>> allPasswords = mBackend.getAllSavedPasswords();
        assertThat(successCallback.getCallCount(), is(1));
        assertThat(allPasswords.get(sTestAccount.get()), hasSize(3));
        assertThat(
                allPasswords, hasEntry(is(sTestAccount.get()), hasItem(updatedPwdWithLocalData)));
    }

    @Test
    public void testUpdateLoginAddsNew() throws TimeoutException {
        fillPasswordStore();

        CallbackHelper successCallback = new CallbackHelper();
        PasswordSpecificsData updatedPasswordData =
                PasswordSpecificsData.newBuilder()
                        .setUsernameValue("Elisa Tester")
                        .setUsernameElement("username")
                        .setPasswordElement("pwd1")
                        .setOrigin("https://accounts.google.com/signin")
                        .setSignonRealm("https://accounts.google.com")
                        .setPasswordValue("UpdatedPassword")
                        .build();
        PasswordWithLocalData updatedPwdWithLocalData =
                PasswordWithLocalData.newBuilder()
                        .setPasswordSpecificsData(updatedPasswordData)
                        .build();
        mBackend.updateLogin(
                updatedPwdWithLocalData.toByteArray(),
                sTestAccount,
                successCallback::notifyCalled,
                unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Map<Account, List<PasswordWithLocalData>> allPasswords = mBackend.getAllSavedPasswords();
        assertThat(successCallback.getCallCount(), is(1));
        assertThat(allPasswords.get(sTestAccount.get()), hasSize(4));
        assertThat(
                allPasswords, hasEntry(is(sTestAccount.get()), hasItem(updatedPwdWithLocalData)));
    }

    @Test
    public void testRemoveLogin() throws TimeoutException {
        fillPasswordStore();

        CallbackHelper successCallback = new CallbackHelper();
        PasswordSpecificsData removedLogin =
                PasswordSpecificsData.newBuilder()
                        .setUsernameValue(sPasswordData.getUsernameValue())
                        .setUsernameElement(sPasswordData.getUsernameElement())
                        .setPasswordElement(sPasswordData.getPasswordElement())
                        .setOrigin(sPasswordData.getOrigin())
                        .setSignonRealm(sPasswordData.getSignonRealm())
                        .build();
        mBackend.removeLogin(
                removedLogin.toByteArray(),
                sTestAccount,
                successCallback::notifyCalled,
                unexpected -> fail());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Map<Account, List<PasswordWithLocalData>> allPasswords = mBackend.getAllSavedPasswords();
        assertThat(successCallback.getCallCount(), is(1));
        assertThat(allPasswords.get(sTestAccount.get()), hasSize(2));
        assertThat(allPasswords, hasEntry(is(sTestAccount.get()), not(hasItem(sPwdWithLocalData))));
    }

    private void fillPasswordStore() {
        mBackend.addLogin(
                sPwdWithLocalData.toByteArray(),
                sTestAccount,
                CallbackUtils.emptyRunnable(),
                unexpected -> fail());
        mBackend.addLogin(
                sPwdWithLocalDataBlocklisted.toByteArray(),
                sTestAccount,
                CallbackUtils.emptyRunnable(),
                unexpected -> fail());
        mBackend.addLogin(
                sPwdWithLocalDataNoOrigin.toByteArray(),
                sTestAccount,
                CallbackUtils.emptyRunnable(),
                unexpected -> fail());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private static @Nullable ListPasswordsResult parseListPasswordResultOrFail(
            byte[] listPwdResult) {
        try {
            return ListPasswordsResult.parseFrom(listPwdResult);
        } catch (Exception parsingError) {
            Assert.fail("Could not parse byte array into ListPasswordsResult.");
            return null;
        }
    }
}
