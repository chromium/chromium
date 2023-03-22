// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.PasswordWithLocalData;
import org.chromium.components.sync.protocol.PasswordSpecificsData;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;

/**
 * Fake {@link PasswordStoreAndroidBackend} to be used in integration tests.
 */
public class FakePasswordStoreAndroidBackend implements PasswordStoreAndroidBackend {
    private final Map<Account, List<PasswordWithLocalData>> mSavedPasswords = new HashMap<>();
    private SequencedTaskRunner mTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.UI_USER_BLOCKING);

    public static final Account sLocalDefaultAccount = new Account("Test user", "Local");

    public FakePasswordStoreAndroidBackend() {
        mSavedPasswords.put(sLocalDefaultAccount, new LinkedList<>());
    }

    public void setSyncingAccount(Account syncingAccount) {
        mSavedPasswords.put(syncingAccount, new LinkedList<>());
    }

    @Override
    public void getAllLogins(Optional<Account> syncingAccount, Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            ListPasswordsResult allLogins =
                    ListPasswordsResult.newBuilder()
                            .addAllPasswordData(mSavedPasswords.get(account))
                            .build();
            loginsReply.onResult(allLogins.toByteArray());
        });
    }

    @Override
    public void getAllLoginsBetween(Date createdAfter, Date createdBefore,
            Optional<Account> syncingAccount, Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            ListPasswordsResult allLogins =
                    ListPasswordsResult.newBuilder()
                            .addAllPasswordData(filterPasswords(mSavedPasswords.get(account),
                                    pwd -> hasDateBetween(pwd, createdAfter, createdBefore)))
                            .build();
            loginsReply.onResult(allLogins.toByteArray());
        });
    };

    @Override
    public void getAutofillableLogins(Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            ListPasswordsResult allLogins =
                    ListPasswordsResult.newBuilder()
                            .addAllPasswordData(filterPasswords(mSavedPasswords.get(account),
                                    pwd -> !pwd.getPasswordSpecificsData().getBlacklisted()))
                            .build();
            loginsReply.onResult(allLogins.toByteArray());
        });
    }

    @Override
    public void getLoginsForSignonRealm(String signonRealm, Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            ListPasswordsResult allLogins =
                    ListPasswordsResult.newBuilder()
                            .addAllPasswordData(filterPasswords(mSavedPasswords.get(account),
                                    pwd -> hasSignonRealm(pwd, signonRealm)))
                            .build();
            loginsReply.onResult(allLogins.toByteArray());
        });
    }

    @Override
    public void addLogin(byte[] pwdWithLocalData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            PasswordWithLocalData parsedPassword =
                    parsePwdWithLocalDataOrFail(pwdWithLocalData, failureCallback);
            if (parsedPassword == null) return;
            assert parsedPassword.getPasswordSpecificsData().hasSignonRealm();
            assert !containsPasswordWithSameUniqueKey(mSavedPasswords.get(account), parsedPassword)
                : "Trying to add password with the same unique key,"
                  + " updateLogin() should be called.";
            mSavedPasswords.get(account).add(parsedPassword);
            successCallback.run();
        });
    }

    @Override
    public void updateLogin(byte[] pwdWithLocalData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            PasswordWithLocalData parsedPassword =
                    parsePwdWithLocalDataOrFail(pwdWithLocalData, failureCallback);
            if (parsedPassword == null) return;
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            assert parsedPassword.getPasswordSpecificsData().hasSignonRealm();
            List<PasswordWithLocalData> accountPasswords = mSavedPasswords.get(account);
            List<PasswordWithLocalData> loginsWithSameUsrAndOrigin =
                    filterPasswords(accountPasswords, pwd -> hasSameUniqueKey(pwd, parsedPassword));
            accountPasswords.removeAll(loginsWithSameUsrAndOrigin);
            assert !containsPasswordWithSameUniqueKey(mSavedPasswords.get(account), parsedPassword);
            accountPasswords.add(parsedPassword);
            successCallback.run();
        });
    }

    @Override
    public void removeLogin(byte[] pwdSpecificsData, Optional<Account> syncingAccount,
            Runnable successCallback, Callback<Exception> failureCallback) {
        mTaskRunner.postTask(() -> {
            PasswordSpecificsData parsedPassword =
                    parsePwdSpecificDataOrFail(pwdSpecificsData, failureCallback);
            if (parsedPassword == null) return;
            Account account = getAccountOrFail(syncingAccount, failureCallback);
            if (account == null) return;
            List<PasswordWithLocalData> pwdsToRemove = filterPasswords(mSavedPasswords.get(account),
                    p -> parsedPassword.equals(p.getPasswordSpecificsData()));
            mSavedPasswords.get(account).removeAll(pwdsToRemove);
            successCallback.run();
        });
    }

    @VisibleForTesting
    public Map<Account, List<PasswordWithLocalData>> getAllSavedPasswords() {
        return mSavedPasswords;
    }

    private static List<PasswordWithLocalData> filterPasswords(
            List<PasswordWithLocalData> list, Predicate<PasswordWithLocalData> predicate) {
        List<PasswordWithLocalData> filteredList = new ArrayList<>();
        for (PasswordWithLocalData pwd : list) {
            if (predicate.test(pwd)) filteredList.add(pwd);
        }
        return filteredList;
    }

    private Account getAccountOrFail(
            Optional<Account> syncingAccount, Callback<Exception> failureCallback) {
        Account account = syncingAccount.isPresent() ? syncingAccount.get() : sLocalDefaultAccount;
        if (!mSavedPasswords.containsKey(account)) {
            failureCallback.onResult(new BackendException(
                    "Account " + account + " not found.", AndroidBackendErrorType.NO_ACCOUNT));
            return null;
        }
        return account;
    }

    private static @Nullable PasswordWithLocalData parsePwdWithLocalDataOrFail(
            byte[] pwdWithLocalData, Callback<Exception> failureCallback) {
        try {
            return PasswordWithLocalData.parseFrom(pwdWithLocalData);
        } catch (Exception parsingError) {
            failureCallback.onResult(parsingError);
            return null;
        }
    }

    private static @Nullable PasswordSpecificsData parsePwdSpecificDataOrFail(
            byte[] pwdWithLocalData, Callback<Exception> failureCallback) {
        try {
            return PasswordSpecificsData.parseFrom(pwdWithLocalData);
        } catch (Exception parsingError) {
            failureCallback.onResult(parsingError);
            return null;
        }
    }

    private static boolean hasDateBetween(
            PasswordWithLocalData pwd, Date createdAfter, Date createdBefore) {
        return pwd.getPasswordSpecificsData().hasDateCreated()
                && pwd.getPasswordSpecificsData().getDateCreated() >= createdAfter.getTime()
                && pwd.getPasswordSpecificsData().getDateCreated() <= createdBefore.getTime();
    }

    private static boolean hasSignonRealm(PasswordWithLocalData pwd, String signonRealm) {
        return pwd.getPasswordSpecificsData().getSignonRealm().contains(signonRealm);
    }

    private static boolean hasSameUniqueKey(
            PasswordWithLocalData pwd, PasswordWithLocalData parsedPassword) {
        PasswordSpecificsData data1 = pwd.getPasswordSpecificsData();
        PasswordSpecificsData data2 = parsedPassword.getPasswordSpecificsData();
        return data1.getUsernameElement().equals(data2.getUsernameElement())
                && data1.getUsernameValue().equals(data2.getUsernameValue())
                && data1.getOrigin().equals(data2.getOrigin())
                && data1.getSignonRealm().equals(data2.getSignonRealm())
                && data1.getPasswordElement().equals(data2.getPasswordElement());
    }

    private static boolean containsPasswordWithSameUniqueKey(
            List<PasswordWithLocalData> list, PasswordWithLocalData pwd) {
        for (PasswordWithLocalData p : list) {
            if (hasSameUniqueKey(p, pwd)) return true;
        }
        return false;
    }
}
